package com.rookie.r

import android.content.Intent
import android.net.VpnService
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Toast
import com.rookie.r.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    companion object {
        init {
            System.loadLibrary("r")
        }
    }

    lateinit var mBinding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        mBinding = ActivityMainBinding.inflate(layoutInflater).also {
            setContentView(it.root)
        }



        mBinding.start.setOnClickListener {

            val intent = VpnService.prepare(this)

            if (intent != null) {
                startActivityForResult(intent, 110)
            } else {
                startVpnService()
            }

        }

        mBinding.stop.setOnClickListener {
            RService.stopService(this)
        }


    }

    fun startVpnService() {
//        val ipAddress = IPUtil.getIPAddress(this)?.map {
//            it to 0
//        }?.toMap()
        val ipAddress = mapOf("10.8.8.8" to 0)
        Logger.log("IP", "$ipAddress")
        if (ipAddress == null) {
            Toast.makeText(this, "network invailable!!", Toast.LENGTH_SHORT).show()
            return
        }

        val appPackages = listOf(
            "com.android.browser",
            "com.android.chrome",
            "com.google.android.youtube",
            "com.rookie.networkclient"
        )
        RService.startService(
            this, VpnConfig(
                "Test Sercvice",
                9000,
                ipAddress,
                mapOf("0.0.0.0" to 0),
                listOf("223.5.5.5"),
                null,
                appPackages
            )
        )
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (requestCode == 110) {
            if (resultCode == RESULT_OK) {
                startVpnService()
            }
        }
    }
}