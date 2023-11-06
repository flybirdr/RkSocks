package com.rookie.r

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.VpnService
import android.os.Build
import android.os.ParcelFileDescriptor
import androidx.core.app.NotificationCompat
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import org.yaml.snakeyaml.DumperOptions
import org.yaml.snakeyaml.Yaml
import java.io.File
import java.io.FileOutputStream
import java.io.IOException


class TunService : VpnService() {

    companion object {

        const val TAG = "VpnService"

        const val NOTIFICATION_NAME = "VpnNotification"
        const val NOTIFICATION_ID = "com.rookie.r.RService"

        const val COMMAND = "key_cmd"
        const val CONFIG = "key_config"

        const val CMD_ON = "on"
        const val CMD_OFF = "off"


        fun startService(context: Context, config: VpnConfig): Intent {
            val intent = Intent(context, TunService::class.java).apply {
                Logger.log(TAG, "before serialization: $config")
                val configJson = Json.encodeToString(config)
                Logger.log(TAG, "after serialization: $configJson")
                putExtra(CONFIG, configJson)
                putExtra(COMMAND, CMD_ON)
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
            return intent;
        }

        fun stopService(context: Context) {
            LocalBroadcastManager.getInstance(context).sendBroadcast(Intent(Constant.ACTION_CLOSE))
        }
    }

    var mConfig: VpnConfig? = null
    var mParcelFileDescriptor: ParcelFileDescriptor? = null

    override fun onCreate() {
        super.onCreate()
        LocalBroadcastManager.getInstance(this).registerReceiver(object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                if (intent?.action == Constant.ACTION_CLOSE) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                        stopForeground(STOP_FOREGROUND_REMOVE)
                    } else {
                        stopForeground(true)
                    }
                    mParcelFileDescriptor?.close()
                    stopSelf()
                }
            }

        }, IntentFilter().apply {
            addAction(Constant.ACTION_CLOSE)
        })
        updateForegroundMessage("正在运行...")
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
        return START_NOT_STICKY
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
            TunService.TAG, """
            configured vpn:
            
            $config
        """.trimIndent()
        )
        return builder.establish()

    }

    fun updateForegroundMessage(message: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.createNotificationChannel(NotificationChannel(NOTIFICATION_ID, NOTIFICATION_NAME, NotificationManager.IMPORTANCE_DEFAULT).apply {
                description = NOTIFICATION_NAME
                enableLights(true)
                enableVibration(false)
                lockscreenVisibility = NotificationCompat.VISIBILITY_PUBLIC
            })
        }
        val builder: NotificationCompat.Builder = NotificationCompat.Builder(this, NOTIFICATION_ID)
        //通知小图标
        //通知小图标
//        builder.setSmallIcon(R.mipmap.ic_launcher)
        //通知标题
        //通知标题
        builder.setContentTitle(NOTIFICATION_NAME)
        //通知内容
        //通知内容
        builder.setContentText("正在运行: $message")
        //设定通知显示的时间
        //设定通知显示的时间
        builder.setWhen(System.currentTimeMillis())
        //设定启动的内容
        //设定启动的内容
        val activityIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(this, 1, activityIntent, PendingIntent.FLAG_UPDATE_CURRENT)
        builder.setContentIntent(pendingIntent)
        val notification = builder.build()

        startForeground(1, notification)
    }

    fun onEstablish(parcelFileDescriptor: ParcelFileDescriptor) {
        val config = mConfig ?: return
        setService(this)
        val tproxy_file = File(cacheDir, "tproxy.conf")
        try {
            tproxy_file.createNewFile()
            val fos = FileOutputStream(tproxy_file, false)
            val yml = mapOf(
                "misc" to mapOf<String, Any>(
                    "task-stack-size" to 20480,
                    "limit-nofile" to 65535
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
    }

    external fun setService(vpnService: VpnService)

    external fun nativeStart(fd: Int, configPath: String)

    external fun nativeStop()
}