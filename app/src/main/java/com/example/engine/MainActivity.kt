package com.example.engine

import android.app.Activity
import android.app.NativeActivity
import android.content.Context
import android.os.Bundle
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.util.Log
import android.view.KeyEvent
import android.view.ViewGroup
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import java.lang.ref.WeakReference

class MainActivity : NativeActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Native code starts via android.app.lib_name in AndroidManifest.
    }

    companion object {
        private const val TAG = "ImeBridge"
        private var editRef: WeakReference<EditText>? = null

        init {
            // Safe even if NativeActivity already loaded it.
            runCatching { System.loadLibrary("engine") }
                .onFailure { Log.w(TAG, "loadLibrary(engine) (ok if already loaded): ${it.message}") }
        }

        // === JNI ===
        @JvmStatic external fun nativeOnCommitText(text: String)

        private fun isComposing(e: Editable): Boolean {
            val s = BaseInputConnection.getComposingSpanStart(e)
            val t = BaseInputConnection.getComposingSpanEnd(e)
            return s != -1 && t != -1 && s != t
        }

        private fun ensureEdit(a: Activity): EditText {
            editRef?.get()?.let { return it }

            val e = EditText(a).apply {
                // Keep it invisible but focusable so IME can attach.
                inputType = InputType.TYPE_CLASS_TEXT or
                        InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
                        InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                isSingleLine = true
                isFocusable = true
                isFocusableInTouchMode = true
                isCursorVisible = false
                alpha = 0f
                layoutParams = ViewGroup.LayoutParams(1, 1)

                imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_ACTION_DONE
            }

            // Send only COMMITTED text; ignore composing updates. Clear after sending.
            e.addTextChangedListener(object : TextWatcher {
                private var suppress = false
                override fun afterTextChanged(s: Editable?) {
                    if (suppress || s == null || s.isEmpty()) return
                    if (isComposing(s)) return   // skip transient composing changes
                    val committed = s.toString()
                    if (committed.isNotEmpty()) {
                        nativeOnCommitText(committed)
                        suppress = true
                        s.clear()                // prevents re-sending the same chunk
                        suppress = false
                    }
                }
                override fun beforeTextChanged(cs: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(cs: CharSequence?, start: Int, before: Int, count: Int) {}
            })

            // Handle Enter and (optionally) Backspace as discrete keys.
            e.setOnEditorActionListener { _, actionId, _ ->
                if (actionId == EditorInfo.IME_ACTION_DONE ||
                    actionId == EditorInfo.IME_ACTION_GO ||
                    actionId == EditorInfo.IME_ACTION_SEND) {
                    nativeOnCommitText("\n")
                    true
                } else {
                    false
                }
            }
            e.setOnKeyListener { _, keyCode, event ->
                if (event.action != KeyEvent.ACTION_DOWN) return@setOnKeyListener false
                when (keyCode) {
                    KeyEvent.KEYCODE_ENTER -> { nativeOnCommitText("\n"); true }
                    // Uncomment if you want backspace forwarded too:
                    // KeyEvent.KEYCODE_DEL   -> { nativeOnCommitText("\u0008"); true }
                    else -> false
                }
            }

            a.runOnUiThread {
                (a.window?.decorView as? ViewGroup)?.addView(e)
                    ?: Log.w(TAG, "No decorView to attach EditText")
                e.requestFocus()
            }
            editRef = WeakReference(e)
            return e
        }

        /** Show IME (no toggle to avoid duplicate show/dismiss churn). */
        @JvmStatic fun imeShow(a: Activity, forced: Boolean) {
            val e = ensureEdit(a)
            a.runOnUiThread {
                e.requestFocus()
                val imm = a.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showSoftInput(
                    e,
                    if (forced) InputMethodManager.SHOW_FORCED else InputMethodManager.SHOW_IMPLICIT
                )
            }
        }

        /** Hide IME. */
        @JvmStatic fun imeHide(a: Activity) {
            val e = editRef?.get() ?: return
            a.runOnUiThread {
                val imm = a.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.hideSoftInputFromWindow(e.windowToken, 0)
                e.clearFocus()
            }
        }
    }
}
