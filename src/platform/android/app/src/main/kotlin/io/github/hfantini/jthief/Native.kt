package io.github.hfantini.jthief

import android.graphics.Bitmap
import java.nio.charset.Charset

/**
 * The game compiled from the repository's C code, through
 * src/platform/android/jni/jni.c. Byte arrays hold the original's
 * Windows-1252 texts; every call must come from the main thread.
 */
object Native {
    init {
        System.loadLibrary("jthief")
    }

    /** Mirrors JtStatus in jni.c. */
    enum class OpenStatus { OK, NOT_ORIGINAL, MALFORMED, MISSING_BITMAP, MISSING_CURSOR, OUT_OF_MEMORY }

    /** Mirrors GAME_EVENT_* in game.h. */
    object Event {
        const val LIFE_LOST = 1 shl 0
        const val EXTRA_LIFE = 1 shl 1
        const val JEWEL_TAKEN = 1 shl 2
        const val LEVEL_UP = 1 shl 3
        const val GAME_OVER = 1 shl 4
        const val GAME_COMPLETED = 1 shl 5
    }

    /** Mirrors AssetsText in assets.h: offsets of texts in the original's data segment. */
    object Text {
        const val TITLE = 0x01D3
        const val TRADE_JEWELS = 0x01DF
        const val PLAY_AGAIN = 0x026A
        const val TRY_EXPERT = 0x0308
    }

    const val SPEED_BEGINNER = 0
    const val SPEED_INTERMEDIATE = 1
    const val SPEED_EXPERT = 2

    const val CURSOR_RIGHT = 0
    const val CURSOR_LEFT = 1

    const val STATUS_TEXTS = 3
    const val HIGH_SCORES = 10

    val ORIGINAL_CHARSET: Charset = Charset.forName("windows-1252")

    fun decode(bytes: ByteArray): String = String(bytes, ORIGINAL_CHARSET)

    fun encode(text: String): ByteArray = text.toByteArray(ORIGINAL_CHARSET)

    /** Opens the original found in `dir`; after NOT_ORIGINAL another file may be tried. */
    fun openOriginal(dir: String): OpenStatus = OpenStatus.entries[open(dir)]

    @JvmStatic private external fun open(dir: String): Int
    @JvmStatic external fun missingName(): String
    @JvmStatic external fun width(): Int
    @JvmStatic external fun height(): Int

    @JvmStatic external fun start(seed: Int)
    @JvmStatic external fun newGame()
    @JvmStatic external fun tick(): Int
    @JvmStatic external fun mouseMove(x: Int, y: Int): Int
    @JvmStatic external fun setFocus(focused: Boolean)
    @JvmStatic external fun level(): Int
    @JvmStatic external fun finalScore(): Int
    @JvmStatic external fun cursorFacesLeft(x: Int, y: Int): Boolean
    @JvmStatic external fun toggleGodMode(): Boolean
    @JvmStatic external fun changeLevel(delta: Int)

    @JvmStatic external fun render(bitmap: Bitmap): Boolean
    @JvmStatic external fun statusText(index: Int): ByteArray
    @JvmStatic external fun statusX(index: Int): Int
    @JvmStatic external fun statusY(index: Int): Int

    @JvmStatic external fun cursorWidth(index: Int): Int
    @JvmStatic external fun cursorHeight(index: Int): Int
    @JvmStatic external fun cursorHotspotX(index: Int): Int
    @JvmStatic external fun cursorHotspotY(index: Int): Int
    @JvmStatic external fun cursorImage(index: Int, bitmap: Bitmap): Boolean

    @JvmStatic external fun text(id: Int): ByteArray
    @JvmStatic external fun dialogText(name: String): ByteArray
    @JvmStatic external fun endingMessage(): ByteArray

    /** Speed, level wait (0 or 1) and cheat (0 or 1), from SERVANT.INI. */
    @JvmStatic external fun loadSettings(): IntArray
    @JvmStatic external fun saveSettings(speed: Int, levelWait: Boolean, cheat: Boolean)

    @JvmStatic external fun loadHighScores()
    @JvmStatic external fun saveHighScores()
    @JvmStatic external fun highScoreRank(score: Int): Int
    @JvmStatic external fun insertHighScore(rank: Int, name: ByteArray, score: Int)
    @JvmStatic external fun highScoreName(index: Int): ByteArray
    @JvmStatic external fun highScoreScore(index: Int): Int
}
