package com.example.app210220

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import com.example.app210220.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
//        setContentView(R.layout.activity_main)
        val binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        var flag = false
        binding.button.setOnClickListener {
            if (flag) {
                binding.textView.text = "A"
                flag = false
            } else {
                binding.textView.text = "B"
                flag = true
            }
        }

    }
}
