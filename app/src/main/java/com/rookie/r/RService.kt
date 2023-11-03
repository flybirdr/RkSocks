package com.rookie.r

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.content.Intent
import android.net.VpnService
import android.os.Build
import android.os.ParcelFileDescriptor
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import org.yaml.snakeyaml.DumperOptions
import org.yaml.snakeyaml.Yaml
import java.io.File
import java.io.FileOutputStream
import java.io.IOException


class RService : VpnService() {

    companion object {

        const val TAG = "VpnService"

        const val NOTIFICATION_NAME = "VpnNotification"
        const val NOTIFICATION_ID = "com.rookie.r.RService"

        const val COMMAND = "key_cmd"
        const val CONFIG = "key_config"

        const val CMD_ON = "on"
        const val CMD_OFF = "off"


        fun startService(context: Context, config: VpnConfig) {

            val intent = Intent(context, RService::class.java).apply {

                Logger.log(TAG, "before serialization: $config")
                val configJson = Json.encodeToString(config)
                Logger.log(TAG, "after serialization: $configJson")

                putExtra(CONFIG, configJson)
                putExtra(COMMAND, CMD_ON)

            }

            context.startService(intent)
        }

        fun stopService(context: Context) {
            val intent = Intent(context, RService::class.java).apply {
                putExtra(COMMAND, CMD_OFF)
            }

            context.stopService(intent)
        }
    }

    var mConfig: VpnConfig? = null

    var mParcelFileDescriptor: ParcelFileDescriptor? = null

    override fun onCreate() {
        super.onCreate()
        Logger.log(TAG, "VpnService created!")

    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {

        //新建立vpn
        if (mParcelFileDescriptor == null) {
            //获取配置
            mConfig = intent?.getStringExtra(CONFIG)?.let { Json.decodeFromString(it) }
            Logger.log(TAG, "got config: $mConfig")

            updateForegroundMessage("configuring...")

            initTunAndServers()

        }
        //重新打开vpn
        else {
            //判断参数是否变化
            val config: VpnConfig? = intent?.getStringExtra(CONFIG)?.let { Json.decodeFromString(it) }

            config?.let {
                if (it != mConfig) {
                    mConfig = it
                    reConfigureTun(it)
                }
            }
        }

        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()
        nativeStop()
    }

    //初始化tun 和 下层tuntosocks 和 socks服务器
    fun initTunAndServers() {
        //初始化tun
        mParcelFileDescriptor = mConfig?.let { configureTun(it) }

        if (mParcelFileDescriptor != null) {
            updateForegroundMessage("configured!!!")
            onEstablish(mParcelFileDescriptor!!)
        }
    }


    fun configureTun(config: VpnConfig): ParcelFileDescriptor? {
        val builder = Builder()
            .apply {
                config.session?.let {
                    setSession(it)
                }
                setMtu(config.mtu)
                config.addresses.forEach {
                    addAddress(it.key, it.value)
                }
                config.routes.forEach {
                    addRoute(it.key, it.value)
                }
                config.dnsServer?.forEach {
                    addDnsServer(it)
                }
                config.searchDomain?.forEach {
                    addSearchDomain(it)
                }
                config.allowedApplications?.forEach {
                    addAllowedApplication(it)
                }

                config?.disallowedApplications?.forEach {
                    addDisallowedApplication(it)
                }

                if (config.allowBypass) {
                    allowBypass()
                }

                setBlocking(false)

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    setMetered(config.metered)
                }
            }

        Logger.log(
            RService.TAG, """
            configured vpn:
            
            $config
        """.trimIndent()
        )
        return builder.establish()

    }

    fun reConfigureTun(config: VpnConfig) {

    }

    fun updateForegroundMessage(message: String) {


        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

            nm.createNotificationChannel(NotificationChannel(NOTIFICATION_ID, NOTIFICATION_NAME, NotificationManager.IMPORTANCE_DEFAULT))
            startForeground(
                1, Notification.Builder(this, NOTIFICATION_ID)
                    .setSmallIcon(R.mipmap.ic_launcher)
                    .setContentText("正在运行: $message")
                    .build()
            )
        }

    }

    fun onEstablish(parcelFileDescriptor: ParcelFileDescriptor) {
        val config = mConfig ?: return
        setCurrentService(this)
        val tproxy_file = File(cacheDir, "tproxy.conf")
        try {
            tproxy_file.createNewFile()
            val fos = FileOutputStream(tproxy_file, false)
            val yml = mapOf(
                "misc" to mapOf<String, Any>(
                    "task-stack-size" to 20480
                ),
                "tunnel" to mapOf<String, Any>(
                    "mtu" to config.mtu
                ),
                "socks5" to mutableMapOf<String, Any>(
                    "port" to config.tun2SocksConfig.socksPort,
                    "address" to config.tun2SocksConfig.socksAddr,
                    "udp" to "udp"
                ).apply {
                    if (!config.tun2SocksConfig.socksUser.isEmpty() &&
                        !config.tun2SocksConfig.socksPassword.isEmpty()
                    ) {
                        put("username", config.tun2SocksConfig.socksUser)
                        put("password", config.tun2SocksConfig.socksPassword)
                    }
                }
            )
            val options = DumperOptions();
            options.setIndent(2);
            options.setPrettyFlow(true);
            options.setDefaultFlowStyle(DumperOptions.FlowStyle.BLOCK);
            val yaml = Yaml(options)
            val yamlStr = yaml.dump(yml)
            Logger.log("Tun2socks", yamlStr)
            fos.write(yamlStr.toByteArray())
            fos.close()
        } catch (e: IOException) {
            return
        }
        nativeStart(parcelFileDescriptor.fd, tproxy_file.absolutePath)

//        thread {
//            val packetLen = mConfig?.mtu ?: return@thread
//            val readBuffer = ByteArray(packetLen)
//            val input = FileInputStream(parcelFileDescriptor.fileDescriptor)
//            while (true) {
//                val read = input.read(readBuffer, 0, readBuffer.size)
//
//                if (read == 0) {
//                    continue
//                }
//                Logger.log("Test", Util.bytes2HexString(readBuffer, read))
//            }
//        }
    }

    external fun setCurrentService(vpnService: VpnService)

    external fun nativeStart(fd: Int, configPath: String)

    external fun nativeStop()
}