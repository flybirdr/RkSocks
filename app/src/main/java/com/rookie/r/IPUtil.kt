package com.rookie.r

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.NetworkInfo
import android.os.Build
import java.net.Inet4Address
import java.net.Inet6Address
import java.net.NetworkInterface

object IPUtil {

    /**
     * 获取所有网络IP地址，除了回环
     */
    fun getIPAddress(context: Context): List<String>? {
        if (!isNetworkAvailable(context)) return null
        val networkInterfaces = NetworkInterface.getNetworkInterfaces()
        val list = ArrayList<String>()
        for (networkInterface in networkInterfaces) {
            if (networkInterface.isLoopback) {
                continue
            }
            val inetAddresses = networkInterface.inetAddresses
            for (inetAddress in inetAddresses) {
                if (inetAddress.isLoopbackAddress) {
                    continue
                }
                if (inetAddress is Inet6Address) {
                    continue
                }
                if (inetAddress is Inet4Address) {
                    inetAddress.hostAddress?.let {
                        list.add(it)
                    }
                }
            }
        }
        if (list.isEmpty()) {
            return null
        } else {
            return list
        }
    }

    /**
     * 判断网络是否可用
     */
    fun isNetworkAvailable(context: Context): Boolean {
        val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val actNet = cm.activeNetwork ?: return false
            val actCap = cm.getNetworkCapabilities(actNet) ?: return false
            return actCap.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) or
                    actCap.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) or
                    actCap.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) or
                    actCap.hasTransport(NetworkCapabilities.TRANSPORT_BLUETOOTH)
        } else {
            val actNet = cm.activeNetworkInfo ?: return false
            return actNet.isConnected
        }

    }
}