package com.rookie.r

import kotlin.math.min

object Util {

    private val hexDigits = charArrayOf('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F')


    fun bytes2HexString(bytes: ByteArray?, len: Int): String {
        if (bytes == null) return ""
        if (len > bytes.size) throw RuntimeException()
        val len = min(bytes.size, len)
        if (len <= 0) return ""
        val ret = CharArray(len shl 1)
        var i = 0
        var j = 0
        while (i < len) {
            ret[j++] = hexDigits.get(bytes[i].toInt() shr 4 and 0x0f)
            ret[j++] = hexDigits.get(bytes[i].toInt() and 0x0f)
            i++
        }
        return String(ret)
    }
}