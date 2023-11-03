package com.rookie.r

import kotlinx.serialization.Serializable


@Serializable
class VpnConfig(
    //服务名称
    var session: String? = null,

    //mtu
    var mtu: Int = 1500,

    //网络地址
    var addresses: Map<String, Int> = HashMap(),

    //路由地址
    var routes: Map<String, Int> = HashMap(),

    //dns
    var dnsServer: List<String>? = null,
    var searchDomain: List<String>? = null,

    //允许通过vpn的应用，默认全部应用走vpn，设置之后只允许指定的应用通过vpn
    var allowedApplications: List<String>? = null,

    //设置不允许通过vpn的应用，默认全部应用走vpn，设置之后只禁止指定的应用通过vpn
    var disallowedApplications: List<String>? = null,

    //所有应用不允许绕过vpn
    var allowBypass: Boolean = false,

    //是否按流量计费
    var metered: Boolean = false,
    //tun2socks配置
    var tun2SocksConfig: Tun2SocksConfig = Tun2SocksConfig()
) {

    override fun toString(): String {
        return "VpnConfig(" +
                "session=$session, " +
                "mtu=$mtu, " +
                "addresses=$addresses, " +
                "routes=$routes, " +
                "dnsServer=$dnsServer, " +
                "searchDomain=$searchDomain, " +
                "allowedApplications=$allowedApplications, " +
                "disallowedApplications=$disallowedApplications, " +
                "allowBypass=$allowBypass, " +
                "metered=$metered)"
    }


}