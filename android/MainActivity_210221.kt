package com.example.app210220

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import com.example.app210220.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {
    private val TAG = MainActivity::class.java.simpleName
    private val COUNT_KEY: String = "COUNT"
    private lateinit var binding: ActivityMainBinding
    private var count: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.button.setOnClickListener {
            count++
            binding.textView.text = count.toString()
        }

    }
    override fun onRestoreInstanceState(savedInstanceState: Bundle?) {
        count = savedInstanceState?.getInt(COUNT_KEY) ?: 0
        binding.textView.text = count.toString()
        savedInstanceState?.let { super.onSaveInstanceState(it) }
    }

    // invoked when the activity may be temporarily destroyed, save the instance state here
    override fun onSaveInstanceState(outState: Bundle) {
        outState.run {
            //putString(GAME_STATE_KEY, gameState)
            //putString(TEXT_VIEW_KEY, textView.text.toString())
            putInt(COUNT_KEY, count)
        }
        // call superclass to save any view hierarchy
        super.onSaveInstanceState(outState)
    }
}
