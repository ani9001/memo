package com.example.app210220

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import com.example.app210220.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        //var ch340isOK = false
        var ch340: UsbDevice? = null
        val ACTION_USB_PERMISSION = BuildConfig.APPLICATION_ID + ".USB_PERMISSION"
        val permissionIntent = PendingIntent.getBroadcast(this, 0, Intent(ACTION_USB_PERMISSION), 0)
        //val device: UsbDevice? = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
        val manager = getSystemService(Context.USB_SERVICE) as UsbManager
        val deviceList: HashMap<String, UsbDevice> = manager.deviceList
        deviceList.values.forEach { device ->
            //your code
            if(device.vendorId == 6790 && device.productId == 29987) { // Vendor ID: 1A86, Device ID: 7523
                if(device.interfaceCount == 1) {
                    manager.requestPermission(device, permissionIntent)
                    binding.textView.text = "ch340-permission-ok"
                    // ch340isOK = true
                    ch340 = device
                }
            }
        }
        val usbReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                if (ACTION_USB_PERMISSION == intent.action) {
                    synchronized(this) {
                        val device: UsbDevice? = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            device?.apply {
                                //call method to set up device communication
                                binding.textView.text = "ch340-receiver-permission-ok"
                                // ch340isOK = true
                                ch340 = device
                            }
                        } else {
                            Log.d("MainActivity", "permission denied for device $device")
                        }
                    }
                }
            }
        }
        val filter = IntentFilter(ACTION_USB_PERMISSION)
        registerReceiver(usbReceiver, filter)

        binding.button.setOnClickListener {
            lateinit var bytes: ByteArray
            val TIMEOUT = 0
            val forceClaim = true
            if (ch340 != null) {
                binding.textView.text = "OK"
                ch340?.getInterface(0)?.also { intf ->
                    intf.getEndpoint(0)?.also { endpoint ->
                        manager.openDevice(ch340)?.apply {
                            binding.textView.text = "open"
                            claimInterface(intf, forceClaim)
                            bytes = "test".toByteArray()
                            //bulkTransfer(endpoint, bytes, bytes.size, TIMEOUT) //do in another thread
                        }
                    }
                }

            } else {
                binding.textView.text = "ch340-NG"
            }
        }

    }
}
