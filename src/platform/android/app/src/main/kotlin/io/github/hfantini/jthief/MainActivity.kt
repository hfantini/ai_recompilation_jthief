package io.github.hfantini.jthief

import android.app.AlertDialog
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.addCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import java.io.File

class MainActivity : ComponentActivity() {
    // The original is kept in the app's private directory, with SERVANT.INI next to it.
    private val original by lazy { File(filesDir, ORIGINAL_NAME) }
    private var game: Game? = null

    private val pickOriginal = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) {
            finish()
        } else {
            copyOriginal(uri)
            openOriginal()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Back pauses, as F3 does on the desktop; the pause box holds the menu.
        onBackPressedDispatcher.addCallback(this) { game?.showPauseMenu() }
        if (original.exists()) {
            openOriginal()
        } else {
            askForOriginal(INTRODUCTION)
        }
    }

    override fun onResume() {
        super.onResume()
        game?.resume()
    }

    override fun onPause() {
        game?.suspend()
        super.onPause()
    }

    override fun onDestroy() {
        game?.destroy()
        super.onDestroy()
    }

    /* Dialogs and the notification shade take the window's focus, as other windows did on Windows. */
    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        game?.setFocus(hasFocus)
        if (hasFocus) {
            hideSystemBars()
        }
    }

    private fun hideSystemBars() {
        WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
    }

    private fun askForOriginal(message: String) {
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.app_name))
            .setMessage(message)
            .setCancelable(false)
            .setPositiveButton("Choose JWLTHIEF.EXE") { _, _ -> pickOriginal.launch(arrayOf("*/*")) }
            .setNegativeButton("Quit") { _, _ -> finish() }
            .show()
    }

    private fun copyOriginal(uri: Uri) {
        contentResolver.openInputStream(uri)?.use { input ->
            original.outputStream().use { output -> input.copyTo(output) }
        }
    }

    private fun openOriginal() {
        when (val status = Native.openOriginal(filesDir.path)) {
            Native.OpenStatus.OK -> startGame()
            Native.OpenStatus.NOT_ORIGINAL -> {
                original.delete()
                askForOriginal(NOT_ORIGINAL)
            }
            else -> showFatalError(openError(status))
        }
    }

    private fun openError(status: Native.OpenStatus): String = when (status) {
        Native.OpenStatus.MALFORMED -> "The original JWLTHIEF.EXE could not be parsed."
        Native.OpenStatus.MISSING_BITMAP -> "Bitmap ${Native.missingName()} is missing from JWLTHIEF.EXE."
        Native.OpenStatus.MISSING_CURSOR -> "Cursor ${Native.missingName()} is missing from JWLTHIEF.EXE."
        else -> "Out of memory."
    }

    private fun showFatalError(message: String) {
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.app_name))
            .setMessage(message)
            .setCancelable(false)
            .setPositiveButton("OK") { _, _ -> finish() }
            .show()
    }

    private fun startGame() {
        val view = GameView(this)

        setContentView(view)
        game = Game(this, view, intent.getBooleanExtra(DEBUG_EXTRA, false)).also {
            it.start()
            it.resume()
            it.setFocus(hasWindowFocus())
        }
    }

    private companion object {
        const val ORIGINAL_NAME = "JWLTHIEF.EXE"

        /*
         * The counterpart of the desktop's --debug:
         * adb shell am start -n io.github.hfantini.jthief/.MainActivity --ez debug true
         */
        const val DEBUG_EXTRA = "debug"
        const val INTRODUCTION =
            "This port plays with the graphics of your own copy of Jewel Thief v1.4. " +
                "Choose its JWLTHIEF.EXE; it is copied into the app and never leaves your device."
        const val NOT_ORIGINAL = "That file is not the original Jewel Thief v1.4 (JWLTHIEF.EXE). Try another one."
    }
}
