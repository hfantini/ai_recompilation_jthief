package io.github.hfantini.jthief

import android.os.Handler
import android.os.Looper
import kotlin.math.ceil
import kotlin.math.max

/**
 * The game clock on the main thread, with the semantics of the other ports'
 * tickers: a fixed period measured against absolute deadlines, so delays do
 * not add up, and late ticks are not caught up.
 */
class Ticker(private val periodMillis: Double, private val onTick: () -> Unit) {
    private val handler = Handler(Looper.getMainLooper())
    private val fire = Runnable { fire() }
    private var next = 0.0
    private var running = false

    fun start() {
        if (running) {
            return
        }
        running = true
        next = now() + periodMillis
        schedule()
    }

    fun stop() {
        running = false
        handler.removeCallbacks(fire)
    }

    private fun now(): Double = System.nanoTime() / 1_000_000.0

    private fun schedule() {
        handler.postDelayed(fire, max(0.0, ceil(next - now())).toLong())
    }

    private fun fire() {
        if (!running) {
            return
        }
        val now = now()

        if (now >= next) {
            onTick()
            next += periodMillis
            // After a stall, resume from now rather than firing a burst.
            if (now - next > periodMillis) {
                next = now + periodMillis
            }
        }
        schedule()
    }
}
