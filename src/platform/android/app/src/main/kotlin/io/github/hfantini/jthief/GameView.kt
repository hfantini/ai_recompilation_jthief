package io.github.hfantini.jthief

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.Typeface
import android.view.MotionEvent
import android.view.View
import kotlin.math.floor

/**
 * Shows the game at the largest whole number of screen pixels per game pixel
 * that fits, centered: any other ratio draws some game pixels wider than
 * others. The status texts are drawn at full resolution so they stay sharp.
 *
 * The whole screen works as a trackpad for the thief, the original's mouse
 * cursor: a finger moves it by as much as the finger moves on the screen,
 * wherever it touches, and lifting the finger leaves it where it is, as
 * letting go of a mouse does. Touching elsewhere never makes it jump.
 */
class GameView(context: Context) : View(context) {
    /** Called with the thief's new position, in game pixels, each time it moves. */
    var onMove: (x: Int, y: Int) -> Unit = { _, _ -> }

    /** Which way the thief looks, as the game decides. */
    var facingLeft = false

    /** Shown in a corner in debug mode, as the desktop shows it in its title bar. */
    var debugLabel: String? = null

    private val frame = Bitmap.createBitmap(Native.width(), Native.height(), Bitmap.Config.ARGB_8888)
    private val thieves = arrayOf(loadCursor(Native.CURSOR_RIGHT), loadCursor(Native.CURSOR_LEFT))
    private val hotspots = Array(2) { intArrayOf(Native.cursorHotspotX(it), Native.cursorHotspotY(it)) }
    private val target = Rect()
    private val thiefTarget = Rect()
    private var scale = 1

    // The trackpad's pointer in game pixels; it starts in the playfield's middle.
    private var pointerX = Native.width() / 2f
    private var pointerY = (Native.height() - STATUS_BAR_HEIGHT) / 2f
    private var sentX = -1
    private var sentY = -1
    private var touchId = MotionEvent.INVALID_POINTER_ID
    private var lastTouchX = 0f
    private var lastTouchY = 0f
    private var moved = false

    // Keeps the pixel art's hard edges when enlarged.
    private val pixelPaint = Paint().apply { isFilterBitmap = false }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.BLACK
        typeface = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD)
    }
    private val hintPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        setShadowLayer(4f, 0f, 0f, Color.BLACK)
    }
    private val debugPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.YELLOW
        textSize = DEBUG_FONT_SIZE * context.resources.displayMetrics.density
    }
    private val texts = Array(Native.STATUS_TEXTS) { "" }

    init {
        setBackgroundColor(Color.BLACK)
    }

    private fun loadCursor(index: Int): Bitmap =
        Bitmap.createBitmap(Native.cursorWidth(index), Native.cursorHeight(index), Bitmap.Config.ARGB_8888)
            .also { Native.cursorImage(index, it) }

    /** Renders the game's current state and schedules it to be shown. */
    fun refresh() {
        Native.render(frame)
        for (index in texts.indices) {
            texts[index] = Native.decode(Native.statusText(index))
        }
        invalidate()
    }

    override fun onSizeChanged(width: Int, height: Int, oldWidth: Int, oldHeight: Int) {
        scale = maxOf(1, minOf(width / frame.width, height / frame.height))
        val left = (width - frame.width * scale) / 2
        val top = (height - frame.height * scale) / 2

        target.set(left, top, left + frame.width * scale, top + frame.height * scale)
        textPaint.textSize = STATUS_FONT_SIZE * scale
        hintPaint.textSize = HINT_FONT_SIZE * scale
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawBitmap(frame, null, target, pixelPaint)
        for (index in texts.indices) {
            val x = target.left + Native.statusX(index) * scale
            val y = target.top + (Native.statusY(index) + SYSTEM_FONT_ASCENT) * scale

            canvas.drawText(texts[index], x.toFloat(), y.toFloat(), textPaint)
        }
        drawThief(canvas)
        if (!moved) {
            canvas.drawText(HINT, target.exactCenterX(), target.top + HINT_TOP * scale.toFloat(), hintPaint)
        }
        debugLabel?.let { canvas.drawText(it, debugPaint.textSize, debugPaint.textSize * 1.5f, debugPaint) }
    }

    private fun drawThief(canvas: Canvas) {
        val index = if (facingLeft) Native.CURSOR_LEFT else Native.CURSOR_RIGHT
        val thief = thieves[index]
        val left = target.left + (floor(pointerX).toInt() - hotspots[index][0]) * scale
        val top = target.top + (floor(pointerY).toInt() - hotspots[index][1]) * scale

        thiefTarget.set(left, top, left + thief.width * scale, top + thief.height * scale)
        canvas.drawBitmap(thief, null, thiefTarget, pixelPaint)
    }

    @SuppressLint("ClickableViewAccessibility") // A trackpad has nothing to click.
    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> startTouch(event, 0)
            MotionEvent.ACTION_POINTER_UP -> {
                // When the tracked finger lifts with another still down, the other takes over.
                val index = event.actionIndex

                if (event.getPointerId(index) == touchId) {
                    startTouch(event, if (index == 0) 1 else 0)
                }
            }
            MotionEvent.ACTION_MOVE -> followTouch(event)
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> touchId = MotionEvent.INVALID_POINTER_ID
        }
        return true
    }

    private fun startTouch(event: MotionEvent, index: Int) {
        touchId = event.getPointerId(index)
        lastTouchX = event.getX(index)
        lastTouchY = event.getY(index)
    }

    /* Every sample Android batched into the event counts, as each mouse move does. */
    private fun followTouch(event: MotionEvent) {
        val index = event.findPointerIndex(touchId)

        if (index < 0) {
            return
        }
        for (sample in 0 until event.historySize) {
            moveBy(event.getHistoricalX(index, sample), event.getHistoricalY(index, sample))
        }
        moveBy(event.getX(index), event.getY(index))
    }

    private fun moveBy(touchX: Float, touchY: Float) {
        // One screen pixel of finger travel is one screen pixel of thief travel.
        pointerX = (pointerX + (touchX - lastTouchX) / scale).coerceIn(0f, frame.width - 1f)
        pointerY = (pointerY + (touchY - lastTouchY) / scale).coerceIn(0f, frame.height - 1f)
        lastTouchX = touchX
        lastTouchY = touchY

        val x = floor(pointerX).toInt()
        val y = floor(pointerY).toInt()

        // Windows reports a mouse move only when the cursor's position changes.
        if (x != sentX || y != sentY) {
            sentX = x
            sentY = y
            moved = true
            onMove(x, y)
        }
    }

    private companion object {
        const val STATUS_BAR_HEIGHT = 32

        /*
         * Close to the Windows 3.1 System font the original prints the status
         * bar with. TextOut places text by the top of the font's 16-pixel cell,
         * whose baseline sits 13 pixels down.
         */
        const val STATUS_FONT_SIZE = 13f
        const val SYSTEM_FONT_ASCENT = 13

        const val HINT = "Drag anywhere on the screen to move the thief"
        const val HINT_FONT_SIZE = 10f
        const val HINT_TOP = 40

        const val DEBUG_FONT_SIZE = 12f
    }
}
