package com.rookie.r

import kotlinx.serialization.Serializable

@Serializable
class Tun2SocksConfig(
    var socksAddr: String = "127.0.0.1",
    var socksPort: Int = 1088,
    var socksUser: String = "rookie",
    var socksPassword: String = "dada",
    var socksDns: String = "8.8.8.8",
) {
    override fun toString(): String {
        return "Tun2SocksConfig(socksAddr='$socksAddr'," +
                "socksPort=$socksPort, " +
                "socksUser='$socksUser', " +
                "socksPassword='$socksPassword', " +
                "socksDns='$socksDns')"
    }
}