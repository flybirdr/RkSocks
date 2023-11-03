package com.rookie.r

import android.util.Log

object Logger {

    fun log(tag: String, content: String) {
        Log.d(tag,content)
    }
}