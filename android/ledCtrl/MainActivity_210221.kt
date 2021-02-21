package com.example.app210220

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.example.app210220.databinding.ActivityMainBinding
import okhttp3.*
import java.io.IOException

class MainActivity : AppCompatActivity() {
    private val TAG = MainActivity::class.java.simpleName
    private lateinit var binding: ActivityMainBinding
    private var state: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.button.setOnClickListener {
            val client: OkHttpClient = OkHttpClient()
            val url: String = "http://xxx.xxx.xxx.xxx/"
            val body: FormBody = FormBody.Builder()
                    .add("light", if (state) "0" else "1")
                    .build()
            val request = Request.Builder().url(url).post(body).build()
            val handler = Handler(Looper.getMainLooper())
            client.newCall(request).enqueue(object: Callback {
                override fun onFailure(call: Call, e: IOException) {
                    Log.d(TAG, "fail : $e")
                }
                override fun onResponse(call: Call, response: Response) {
                    if (response.code == 200) {
                        state = !state
                        handler.post { binding.textView.text = if (state) "ON" else "OFF" }
                    }
                }
            })
        }
    }
}
