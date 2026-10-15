package io.github.hfantini.jthief

import android.app.AlertDialog
import android.content.Context
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.text.InputFilter
import android.view.Gravity
import android.view.View
import android.widget.CheckBox
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TableLayout
import android.widget.TableRow
import android.widget.TextView

/** The game's options, as stored in SERVANT.INI. */
data class Settings(val speed: Int, val levelWait: Boolean, val cheat: Boolean)

/** A command of the pause menu. */
class MenuItem(val label: String, val run: () -> Unit)

/**
 * The game's dialogs as native Android dialogs. A dialog takes the window's
 * focus, which deactivates the game as the original's modal dialogs did;
 * the clock keeps running meanwhile. Their layout is this port's own; the
 * long texts come from the original.
 */
class Dialogs(private val context: Context, private val title: String) {
    // The original's texts break lines with CR LF.
    private fun clean(text: String) = text.replace("\r", "")

    private fun dp(value: Int) = (value * context.resources.displayMetrics.density).toInt()

    private fun padded(view: View) = view.apply { setPadding(dp(24), dp(8), dp(24), 0) }

    fun message(text: String, done: () -> Unit = {}) = inform(title, text, done)

    private fun inform(caption: String, text: String, done: () -> Unit = {}) {
        AlertDialog.Builder(context)
            .setTitle(caption)
            .setMessage(clean(text))
            .setPositiveButton("OK", null)
            .setOnDismissListener { done() }
            .show()
    }

    fun question(text: String, answered: (Boolean) -> Unit) {
        var answer = false

        AlertDialog.Builder(context)
            .setTitle(title)
            .setMessage(clean(text))
            .setPositiveButton("Yes") { _, _ -> answer = true }
            .setNegativeButton("No", null)
            .setOnDismissListener { answered(answer) }
            .show()
    }

    /** The pause box, which on a phone also holds the original's menu commands. */
    fun pauseMenu(items: List<MenuItem>) {
        AlertDialog.Builder(context)
            .setTitle("Paused")
            .setItems(items.map { it.label }.toTypedArray()) { _, which -> items[which].run() }
            .show()
    }

    /** "Get ready...", closing itself after 3 seconds. */
    fun getReady() {
        val dialog = AlertDialog.Builder(context)
            .setTitle("Get ready")
            .setMessage("Get ready...")
            .setCancelable(false)
            .show()

        Handler(Looper.getMainLooper()).postDelayed({ dialog.dismiss() }, WAIT_MILLISECONDS)
    }

    /** Calls `done` with the new settings, or with null if the player cancels. */
    fun options(current: Settings, onHelp: () -> Unit, done: (Settings?) -> Unit) {
        val speeds = RadioGroup(context)
        val speedIds = listOf(Native.SPEED_BEGINNER, Native.SPEED_INTERMEDIATE, Native.SPEED_EXPERT)
            .zip(listOf("Beginner", "Intermediate", "Expert"))
            .associate { (speed, label) ->
                val button = RadioButton(context).apply {
                    id = View.generateViewId()
                    text = label
                }
                speeds.addView(button)
                button.id to speed
            }
        val levelWait = CheckBox(context).apply {
            text = "Pause after each level"
            isChecked = current.levelWait
        }
        val cheat = CheckBox(context).apply {
            text = "Cheat mode: the thief can leave the playing field"
            isChecked = current.cheat
        }
        val body = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            addView(TextView(context).apply { text = "Speed" })
            addView(speeds)
            addView(levelWait)
            addView(cheat)
        }
        var result: Settings? = null

        speeds.check(speedIds.entries.first { it.value == current.speed }.key)
        AlertDialog.Builder(context)
            .setTitle("Options")
            .setView(padded(body))
            .setPositiveButton("OK") { _, _ ->
                result = Settings(speedIds.getValue(speeds.checkedRadioButtonId), levelWait.isChecked, cheat.isChecked)
            }
            .setNegativeButton("Cancel", null)
            // The original's Help button opened WinHelp; the instructions box stands in.
            .setNeutralButton("Help", null)
            .setOnDismissListener { done(result) }
            .create()
            .apply {
                setOnShowListener {
                    // Set here so the options stay open under the help.
                    getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener { onHelp() }
                }
            }
            .show()
    }

    fun highScores(names: List<String>, scores: List<Int>, done: () -> Unit = {}) {
        val table = TableLayout(context).apply { setColumnStretchable(2, true) }

        names.indices.forEach { index ->
            val row = TableRow(context)

            row.addView(cell("${index + 1}.", Gravity.END))
            row.addView(cell(scores[index].toString(), Gravity.END))
            row.addView(cell(names[index], Gravity.START))
            table.addView(row)
        }
        AlertDialog.Builder(context)
            .setTitle("High Scores")
            .setView(padded(table))
            .setPositiveButton("OK", null)
            .setOnDismissListener { done() }
            .show()
    }

    private fun cell(text: String, gravity: Int) = TextView(context).apply {
        this.text = text
        this.gravity = gravity
        typeface = Typeface.MONOSPACE
        setPadding(dp(4), dp(2), dp(12), dp(2))
    }

    /** Calls `done` with the name entered, empty if the player cancels. */
    fun newHighScore(score: Int, done: (String) -> Unit) {
        val input = EditText(context).apply {
            isSingleLine = true
            filters = arrayOf(InputFilter.LengthFilter(NAME_LENGTH))
        }
        val body = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            addView(TextView(context).apply { text = "Your score of $score is in the top ten.\nEnter your name:" })
            addView(input)
        }
        var name = ""

        AlertDialog.Builder(context)
            .setTitle("New High Score")
            .setView(padded(body))
            .setPositiveButton("OK") { _, _ -> name = input.text.toString() }
            .setNegativeButton("Cancel", null)
            .setOnDismissListener { done(name) }
            .show()
        input.requestFocus()
    }

    fun about() = inform(
        "About Jewel Thief",
        "Jewel Thief v1.4\nOriginal game © 1991–1993 ServantWare.\n\n" +
            "Android port. The graphics and texts are read from your own copy of JWLTHIEF.EXE, " +
            "which never leaves your device.",
    )

    /** One of the original's help boxes; nothing shows if the original lacks it. */
    fun help(caption: String, text: String) {
        if (text.isNotEmpty()) {
            inform(caption, text)
        }
    }

    private companion object {
        const val WAIT_MILLISECONDS = 3000L
        const val NAME_LENGTH = 25
    }
}
