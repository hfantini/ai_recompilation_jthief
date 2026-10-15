package io.github.hfantini.jthief

import android.app.Activity
import android.media.AudioManager
import android.media.ToneGenerator

/**
 * The game flow around the rules, ported from the GTK entry point: what to
 * do after each tick and move, the "Get ready" pauses, the end of a game
 * with its high scores, and the menu commands, which the pause box holds on
 * a phone. Only the gameplay follows the original; the dialogs are this
 * port's own.
 *
 * In debug mode, the counterpart of the desktop's --debug, the pause box
 * also offers what the desktop's debug keys do.
 */
class Game(private val activity: Activity, private val view: GameView, private val debug: Boolean) {
    private val ticker = Ticker(TICK_MILLISECONDS) { handleTick() }
    private val dialogs = Dialogs(activity, Native.decode(Native.text(Native.Text.TITLE)))
    private val beeper = ToneGenerator(AudioManager.STREAM_MUSIC, BEEP_VOLUME)
    private var godMode = false

    /*
     * Startup as in the original, minus the shareware About screen: the game
     * is dealt before it shows, and the "Get ready" box may follow.
     */
    fun start() {
        // Like srand(time(NULL)): only the low 16 bits of the time are used.
        Native.start((System.currentTimeMillis() / 1000).toInt() and 0xFFFF)
        view.onMove = ::handleMove
        updateDebugLabel()
        view.refresh()
        waitIfEnabled()
    }

    fun resume() = ticker.start()

    fun suspend() = ticker.stop()

    fun destroy() {
        ticker.stop()
        beeper.release()
    }

    /** The original's window is inactive under its dialogs, and so is the game then. */
    fun setFocus(focused: Boolean) = Native.setFocus(focused)

    fun showPauseMenu() {
        val items = mutableListOf(
            MenuItem("Play") {},
            MenuItem("New...", ::requestNewGame),
            MenuItem("Options...", ::showOptions),
            MenuItem("High Scores") { showHighScores() },
            MenuItem("Instructions") { showHelp("Instructions", "INSTRBOX") },
            MenuItem("Hints") { showHelp("Hints", "HINTSBOX") },
            MenuItem("Our Story") { showHelp("Our Story", "STORYBOX") },
            MenuItem("About Jewel Thief...", dialogs::about),
        )

        if (debug) {
            items += MenuItem(if (godMode) "Debug: god mode off" else "Debug: god mode on", ::toggleGodMode)
            items += MenuItem("Debug: next level") { changeLevel(1) }
            items += MenuItem("Debug: previous level") { changeLevel(-1) }
        }
        items += MenuItem("Exit", activity::finish)
        dialogs.pauseMenu(items)
    }

    private fun handleTick() {
        handleEvents(Native.tick())
        view.refresh()
    }

    private fun handleMove(x: Int, y: Int) {
        val events = Native.mouseMove(x, y)

        // The original turns the cursor before handling a jewel or a level-up.
        view.facingLeft = Native.cursorFacesLeft(x, y)
        handleEvents(events)
        view.invalidate()
    }

    private fun handleEvents(events: Int) {
        if (events and Native.Event.LIFE_LOST != 0) {
            beeper.startTone(ToneGenerator.TONE_PROP_BEEP, BEEP_MILLISECONDS)
        }
        if (events and (Native.Event.GAME_OVER or Native.Event.GAME_COMPLETED) != 0) {
            finishGame(events)
            return
        }
        if (events != 0) {
            view.refresh()
        }
        // A new scenery comes every second level; the original may pause there.
        if (events and Native.Event.LEVEL_UP != 0 && Native.level() % 2 == 0) {
            waitIfEnabled()
        }
    }

    /* Settings are read each time, as the original reads SERVANT.INI each time. */
    private fun loadSettings(): Settings {
        val (speed, levelWait, cheat) = Native.loadSettings()

        return Settings(speed, levelWait != 0, cheat != 0)
    }

    private fun waitIfEnabled() {
        if (loadSettings().levelWait) {
            dialogs.getReady()
        }
    }

    private fun newGame() {
        Native.newGame()
        view.refresh()
        waitIfEnabled()
    }

    private fun requestNewGame() {
        dialogs.question(Native.decode(Native.text(Native.Text.TRADE_JEWELS))) { yes ->
            if (yes) newGame()
        }
    }

    private fun showOptions() {
        dialogs.options(loadSettings(), onHelp = { showHelp("Instructions", "INSTRBOX") }) { settings ->
            if (settings != null) {
                Native.saveSettings(settings.speed, settings.levelWait, settings.cheat)
                newGame()
            }
        }
    }

    /* The instructions, hints and story boxes, unused by v1.4 but kept in it. */
    private fun showHelp(caption: String, templateName: String) =
        dialogs.help(caption, Native.decode(Native.dialogText(templateName)))

    /** Shows the table already loaded into the game. */
    private fun showLoadedHighScores(done: () -> Unit) {
        val names = List(Native.HIGH_SCORES) { Native.decode(Native.highScoreName(it)) }
        val scores = List(Native.HIGH_SCORES) { Native.highScoreScore(it) }

        dialogs.highScores(names, scores, done)
    }

    private fun showHighScores(done: () -> Unit = {}) {
        Native.loadHighScores()
        showLoadedHighScores(done)
    }

    private fun finishGame(events: Int) {
        view.refresh()
        if (events and Native.Event.GAME_COMPLETED != 0) {
            showEnding { submitHighScore(::askPlayAgain) }
        } else {
            submitHighScore(::askPlayAgain)
        }
    }

    private fun showEnding(done: () -> Unit) {
        val expert = loadSettings().speed == Native.SPEED_EXPERT
        val text = if (expert) Native.endingMessage() else Native.text(Native.Text.TRY_EXPERT)

        dialogs.message(Native.decode(text), done)
    }

    /*
     * Asks for a name if the score makes the table, then shows the table. The
     * table is saved even when the score misses it, as the original does.
     */
    private fun submitHighScore(done: () -> Unit) {
        val score = Native.finalScore()

        Native.loadHighScores()
        if (Native.highScoreRank(score) < 0) {
            Native.saveHighScores()
            showLoadedHighScores(done)
            return
        }
        dialogs.newHighScore(score) { name ->
            // Read again, as the original does once the name is entered.
            Native.loadHighScores()
            Native.insertHighScore(Native.highScoreRank(score), Native.encode(name), score)
            Native.saveHighScores()
            showLoadedHighScores(done)
        }
    }

    private fun askPlayAgain() {
        dialogs.question(Native.decode(Native.text(Native.Text.PLAY_AGAIN))) { yes ->
            if (yes) newGame() else activity.finish()
        }
    }

    private fun toggleGodMode() {
        godMode = Native.toggleGodMode()
        updateDebugLabel()
    }

    private fun changeLevel(delta: Int) {
        Native.changeLevel(delta)
        view.refresh()
    }

    /* The desktop shows these in its title bar. */
    private fun updateDebugLabel() {
        view.debugLabel = when {
            !debug -> null
            godMode -> "[DEBUG: god mode]"
            else -> "[DEBUG]"
        }
        view.invalidate()
    }

    private companion object {
        /*
         * Windows 3.1 timers ran on the PC timer chip at 1193182 / 65536 Hz,
         * so the game's 5 ms timer really fired every ~54.9 ms; that is the
         * game's pace.
         */
        const val TICK_MILLISECONDS = 65536.0 / 1193182.0 * 1000.0

        const val BEEP_VOLUME = 60
        const val BEEP_MILLISECONDS = 120
    }
}
