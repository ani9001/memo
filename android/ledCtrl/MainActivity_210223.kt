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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.button.setOnClickListener {
            val client: OkHttpClient = OkHttpClient()
            val handler = Handler(Looper.getMainLooper())
            fun defaultOnFail(call: Call, e: IOException) = Log.d(TAG, "fail : $e")

            fun postNewCall(url: String, body: FormBody, onFail: (Call, IOException) -> Unit, onResp: (Call, Response) -> Unit) {
                val request = Request.Builder().url(url).post(body).build()
                client.newCall(request).enqueue(object: Callback {
                    override fun onFailure(call: Call, e: IOException) = onFail(call, e)
                    override fun onResponse(call: Call, response: Response) = onResp(call, response)
                })
            }

            val body: FormBody = FormBody.Builder()
                    .add("arp", "1")
                    .build()
            postNewCall(getString(R.string.clockUrl), body, ::defaultOnFail, fun(call: Call, response: Response) {
                if (response.code == 200) {
                    val body: FormBody = FormBody.Builder()
                            .add("pin", "4")
                            .add("duty", "100")
                            .add("freq", "32")
                            .build()
                    postNewCall(getString(R.string.envUrl), body, ::defaultOnFail, fun(call: Call, response: Response) {
                        if (response.code == 200) {
                            val body: FormBody = FormBody.Builder()
                                    .add("svc", "close")
                                    .build()
                            postNewCall(getString(R.string.envUrl), body, ::defaultOnFail, fun(call: Call, response: Response) {
                                if (response.code == 200) {
                                    handler.post { binding.textView.text = "OK" }
                                }
                            })
                        }
                    })
                }
            })
        }
    }
}
