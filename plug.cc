#include <asiolink/io_address.h>
#include <hooks/hooks.h>
#include <dhcp/pkt4.h>
#include <dhcp/pkt6.h>
#include <dhcp/dhcp4.h>
#include <dhcp/dhcp6.h>
#include <dhcp/option_string.h>
#include <dhcp/pkt4.h>
#include <dhcp/pkt6.h>
#include <dhcpsrv/lease.h>
#include <log/message_initializer.h>
#include <log/macros.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <boost/utility.hpp>

#include <curl/curl.h>

#include <iostream>
#include <fstream>
#include <map>
#include <errno.h>

#include "plug_messages.h"

using namespace isc::hooks;
using namespace isc::dhcp;

struct ProvisionerData {
    bool deny;
    std::string ipv4;
    std::string next_server;
    std::map<uint8_t, std::string> options;
};

const char* query_hwaddr_label = "mr_provisioner_hwaddr";
const char* query_duid_label = "mr_provisioner_duid";

long http_timeout_ms = 5000L;

std::string provisioner_url;

isc::log::Logger plug_logger("mr-provisioner");

extern "C" {
size_t write_fn(void *ptr, size_t size, size_t nmemb, std::stringstream* data) {
    data->write((char*) ptr, size * nmemb);
    return size * nmemb;
}
}

int do_req(bool post, std::stringstream& response_ss, const char *url, const std::string& body = "") {
    struct curl_slist *headers = NULL;
    CURL *curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR(plug_logger, PLUG_HOOK_NET_REQUEST_CURL_ERR)
            .arg("curl_easy_init() failed");

        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    headers = curl_slist_append(headers, "Accept: application/json");

    if (post) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);

    std::stringstream header_ss;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_fn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_ss);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_ss);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, http_timeout_ms);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        LOG_ERROR(plug_logger, PLUG_HOOK_NET_REQUEST_CURL_ERR)
            .arg(curl_easy_strerror(res));

        return -1;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code >= 300) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        LOG_WARN(plug_logger, PLUG_HOOK_NET_REQUEST_HTTP_ERR)
            .arg(response_code);

        return -1;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return 0;
}

int fetch_provisioner_data(ProvisionerData& pd, const std::string& hwaddr, bool ipv4) {
    std::stringstream response_ss;

    std::string url = provisioner_url + (ipv4 ? "/ipv4" : "/ipv6") + "?hwaddr=" + hwaddr;

    LOG_INFO(plug_logger, PLUG_HOOK_NET_REQUEST)
        .arg(url);

    if (do_req(false, response_ss, url.c_str()) != 0) {
        return -1;
    }

    boost::property_tree::ptree pt_body;
    boost::property_tree::json_parser::read_json(response_ss, pt_body);

    pd.deny = false;
    pd.ipv4 = pt_body.get<std::string>("ipv4", "");
    pd.next_server = pt_body.get<std::string>("next-server", "");

    auto& pt_opts = pt_body.get_child("options");
    for (auto& child : pt_opts) {
        uint8_t opt_code = child.second.get<uint8_t>("option");
        std::string opt_value = child.second.get<std::string>("value");

        pd.options[opt_code] = opt_value;
    }

    return 0;
}

int post_provisioner_data(const std::string& path, boost::property_tree::ptree &pt) {
    std::stringstream response_ss;
    std::ostringstream body_ss;

    std::string url = provisioner_url + path;

    boost::property_tree::json_parser::write_json(body_ss, pt);
    std::string body_s = body_ss.str();

    LOG_INFO(plug_logger, PLUG_HOOK_NET_REQUEST)
        .arg(url);

    if (do_req(true, response_ss, url.c_str(), body_s) != 0) {
        return -1;
    }

    return 0;
}

void add_option4(Pkt4Ptr& response, uint8_t opt_code, std::string& opt_value) {
    // Remove the option if it exists.
    OptionPtr opt = response->getOption(opt_code);
    if (opt) {
        response->delOption(opt_code);
    }

    // Now add the option.
    opt.reset(new OptionString(Option::V4, opt_code, opt_value));
    response->addOption(opt);
}

extern "C" {
// Version function required by Hooks API for compatibility checks.
int version() {
    return (KEA_HOOKS_VERSION);
}

// Called by the Hooks library manager when the library is loaded.
int load(LibraryHandle& handle) {
    try {
        curl_global_init(CURL_GLOBAL_ALL);

        auto purl_ptr = handle.getParameter("provisioner_url");
        if (!isc::data::isNull(purl_ptr)) {
            provisioner_url = purl_ptr->stringValue();
        } else {
            LOG_ERROR(plug_logger, PLUG_HOOK_PARAMETER_ERR)
                .arg("provisioner_url")
                .arg("not provided");

            return 1;
        }

        auto timeout_ptr = handle.getParameter("timeout_ms");
        if (!isc::data::isNull(timeout_ptr)) {
            http_timeout_ms = (long)timeout_ptr->intValue();
        }

    } catch (const std::exception& ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("load")
            .arg(ex.what());

        return 1;
    }

    return 0;
}

// Called by the Hooks library manager when the library is unloaded.
int unload() {
    try {
    } catch (const std::exception& ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("unload")
            .arg(ex.what());
    }

    return 0;
}

int post_discovery4(Pkt4Ptr& query, HWAddrPtr& hwaddr) {
    boost::property_tree::ptree pt;
    boost::property_tree::ptree pt_opts;

    isc::dhcp::OptionCollection& opts = query->options_;
    for (auto& p : opts) {
        boost::property_tree::ptree pt_opt;

        pt_opt.put("option", p.first);

        if (p.first == 12) {
            // String values
            std::stringstream ss;
            const isc::dhcp::OptionBuffer& opt_buf = p.second->getData();
            ss.write((const char *)&opt_buf[0], opt_buf.size());
            ss.write("\0", 1);
            pt_opt.put("value", ss.str());
            pt_opts.push_back(std::make_pair("", pt_opt));
        } else if (p.first == 93) {
            // uint16 values
            uint16_t value = p.second->getUint16();
            pt_opt.put("value", value);
            pt_opts.push_back(std::make_pair("", pt_opt));
        }
    }

    // discover: true/false
    // mac
    // options: { option:, value: ""}
    pt.put("discover", (query->getType() == DHCPDISCOVER));
    pt.put("mac", hwaddr->toText(false));
    pt.add_child("options", pt_opts);

    return post_provisioner_data("/ipv4/seen", pt);
}

int post_lease4(Pkt4Ptr& response, HWAddrPtr& hwaddr) {
    boost::property_tree::ptree pt;
    uint32_t duration_secs = 0U;

    const isc::asiolink::IOAddress& siaddr = response->getSiaddr();

    const auto& it = response->options_.find(51U);
    if (it != response->options_.end()) {
        duration_secs = it->second->getUint32();
    }

    pt.put("mac", hwaddr->toText(false));
    pt.put("ipv4", siaddr.toText());
    pt.put("duration", duration_secs);
    // mac
    // ipv4
    // duration (opt 51)

    return post_provisioner_data("/ipv4/lease", pt);
}

int pkt4_receive(CalloutHandle& handle) {
    try {
        // Get the HWAddress to use as the user identifier.
        Pkt4Ptr query;
        handle.getArgument("query4", query);
        uint8_t packet_type = query->getType();
        HWAddrPtr hwaddr = query->getMAC(HWAddr::HWADDR_SOURCE_ANY);

        // Store the id we search with so it is available down the road.
        handle.setContext(query_hwaddr_label, hwaddr);

        if (packet_type == DHCPDISCOVER || packet_type == DHCPREQUEST) {
            post_discovery4(query, hwaddr);
        }
    } catch (const std::exception& ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("pkt4_receive")
            .arg(ex.what());

        return 1;
    }

    return (0);
}

#if 0
int pkt6_receive(CalloutHandle& handle) {
    try {
        // Fetch the inbound packet.
        Pkt6Ptr query;
        handle.getArgument("query6", query);

        HWAddrPtr hwaddr = query->getMAC(HWAddr::HWADDR_SOURCE_ANY);

        // Store the id we search with so it is available down the road.
        handle.setContext(query_hwaddr_label, hwaddr);
    } catch (const std::exception& ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("pkt6_receive")
            .arg(ex.what());

        return 1;
    }

    return (0);
}
#endif

int lease4_select(CalloutHandle& handle) {
    try {
        Lease4Ptr lease;
        handle.getArgument("lease4", lease);

        // Get the user id saved from the query packet.
        HWAddrPtr hwaddr;
        handle.getContext(query_hwaddr_label, hwaddr);

        ProvisionerData pd;
        if (fetch_provisioner_data(pd, hwaddr->toText(false), true) != 0) {
            return 0;
        }

        if (pd.ipv4 != "") {
            const isc::asiolink::IOAddress new_addr(pd.ipv4);

            lease->addr_ = new_addr;

            LOG_INFO(plug_logger, PLUG_HOOK_OVERRIDE_IP)
                .arg(hwaddr->toText(false))
                .arg(pd.ipv4);
        }
    } catch (const std::exception &ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("lease4_select")
            .arg(ex.what());

        return 1;
    }

    return 0;
}

int lease4_renew(CalloutHandle& handle) {
    try {
        Lease4Ptr lease;
        handle.getArgument("lease4", lease);

        // Get the user id saved from the query packet.
        HWAddrPtr hwaddr;
        handle.getContext(query_hwaddr_label, hwaddr);

        ProvisionerData pd;
        if (fetch_provisioner_data(pd, hwaddr->toText(false), true) != 0) {
            return 0;
        }

        if (pd.ipv4 != "") {
            const isc::asiolink::IOAddress new_addr(pd.ipv4);

            lease->addr_ = new_addr;

            LOG_INFO(plug_logger, PLUG_HOOK_OVERRIDE_IP)
                .arg(hwaddr->toText(false))
                .arg(pd.ipv4);
        }
    } catch (const std::exception &ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("lease4_renew")
            .arg(ex.what());

        return 1;
    }

    return 0;
}

int pkt4_send(CalloutHandle& handle) {
    try {
        Pkt4Ptr response;
        handle.getArgument("response4", response);

        uint8_t packet_type = response->getType();
        if (packet_type == DHCPNAK) {
            std::cout << "DHCP UserCheckHook : pkt4_send"
                      << "skipping packet type: "
                      << static_cast<int>(packet_type) << std::endl;
            return (0);
        }

        // Get the user id saved from the query packet.
        HWAddrPtr hwaddr;
        handle.getContext(query_hwaddr_label, hwaddr);

#if 0
        // Fetch the lease address.
        isc::asiolink::IOAddress addr = response->getYiaddr();
#endif

        ProvisionerData pd;
        if (fetch_provisioner_data(pd, hwaddr->toText(false), true) != 0) {
            return 0;
        }

        if (pd.next_server != "") {
            const isc::asiolink::IOAddress new_addr(pd.next_server);

            response->setSiaddr(new_addr);
        }

        for (auto& p : pd.options) {
            add_option4(response, p.first, p.second);

            LOG_INFO(plug_logger, PLUG_HOOK_SET_OPTION)
                .arg(hwaddr->toText(false))
                .arg(p.first)
                .arg(p.second);
        }
    } catch (const std::exception& ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("pkt4_send")
            .arg(ex.what());

        return 1;
    }

    return (0);
}

#if 0
int pkt6_send(CalloutHandle& handle) {
    try {
        Pkt6Ptr response;
        handle.getArgument("response6", response);

        // Fetch the lease address as a string
        std::string addr_str = getV6AddrStr(response);
        if (addr_str.empty()) {
            // packet did not contain an address, must be failed.
            std::cout << "pkt6_send: Skipping packet address is blank"
                      << std::endl;
            return (0);
        }

        // Get the user id saved from the query packet.
        HWAddrPtr hwaddr;
        handle.getContext(query_hwaddr_label, hwaddr);

    } catch (const std::exception& ex) {
        LOG_ERROR(plug_logger, PLUG_HOOK_UNEXPECTED_ERROR)
            .arg("pkt6_send")
            .arg(ex.what());

        return 1;
    }

    return (0);
}
#endif
}
