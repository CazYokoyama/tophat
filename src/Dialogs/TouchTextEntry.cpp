/*
Copyright_License {

  Top Hat Soaring Glide Computer - http://www.tophatsoaring.org/
  Copyright (C) 2000-2016 The Top Hat Soaring Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Dialogs/TextEntry.hpp"
#include "Look/DialogLook.hpp"
#include "Form/Form.hpp"
#include "Form/Button.hpp"
#include "Form/Edit.hpp"
#include "Form/LambdaActionListener.hpp"
#include "Widget/KeyboardWidget.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Key.h"
#include "Util/StringUtil.hpp"
#include "Util/NumberParser.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "Dialogs/HelpDialog.hpp"

#include <algorithm>
#include <assert.h>

static WndProperty *editor;
static KeyboardWidget *kb = nullptr;

static AllowedCharacters AllowedCharactersCallback;

static constexpr size_t MAX_TEXTENTRY = 40;
static unsigned int cursor = 0;
static size_t max_width;
static TCHAR edittext[MAX_TEXTENTRY];

/**
 * has the value been edited by the user
 */
bool has_been_edited;

static void ClearText();

/**
 * clears the field and marks the field as having been modified
 */
static void
ClearTextFirstTime()
{
  ClearText();
  has_been_edited = true;
}

static void
UpdateAllowedCharacters()
{
  if (AllowedCharactersCallback)
    kb->SetAllowedCharacters(AllowedCharactersCallback(edittext));
}

static void
UpdateTextboxProp()
{
  editor->SetHighlight(!has_been_edited);
  editor->SetText(edittext);

  UpdateAllowedCharacters();
}

static bool
DoBackspace()
{
  if (!has_been_edited)
    ClearTextFirstTime();

  if (cursor < 1)
    return false;

  cursor--;
  edittext[cursor] = 0;
  UpdateTextboxProp();
  return true;
}

static void
OnBackspace()
{
  DoBackspace();
}

static bool
DoCharacter(TCHAR character)
{
  if (!has_been_edited)
    ClearTextFirstTime();

  if (cursor >= max_width - 1)
    return false;

  edittext[cursor++] = character;
  edittext[cursor] = 0;
  UpdateTextboxProp();
  return true;
}

static bool
FormKeyDown(unsigned key_code)
{
  switch (key_code) {
  case KEY_RIGHT:
    return true;
  case KEY_LEFT:
  case KEY_BACK:
    DoBackspace();
    return true;
  }

  return false;
}

static bool
FormCharacter(unsigned ch)
{
  if (ch < 0x20)
    return false;

#ifndef _UNICODE
  if (ch >= 0x80)
    /* TODO: ASCII only for now, because we don't have proper UTF-8
       support yet */
    return false;
#endif

  DoCharacter((TCHAR)ch);
  return true;
}

static void
ClearText()
{
  cursor = 0;
  edittext[0] = 0;
  UpdateTextboxProp();
}

bool
TouchTextEntry(TCHAR *text, size_t width,
               const TCHAR *caption,
               AllowedCharacters accb,
               bool show_shift_key,
               bool default_shift_state)
{
  if (width == 0)
    width = MAX_TEXTENTRY;

  has_been_edited = true; // disable clearing of value on first key for alpha

  max_width = std::min(MAX_TEXTENTRY, width);

  const DialogLook &look = UIGlobals::GetDialogLook();
  WndForm form(look);
  form.Create(UIGlobals::GetMainWindow(), caption);
  form.SetKeyDownFunction(FormKeyDown);
  form.SetCharacterFunction(FormCharacter);

  ContainerWindow &client_area = form.GetClientAreaWindow();
  const PixelRect rc = client_area.GetClientRect();

  const PixelScalar client_height = rc.bottom - rc.top;

  const PixelScalar padding = Layout::Scale(2);
  const PixelScalar backspace_width = Layout::Scale(46);
  const PixelScalar backspace_left = rc.right - padding - backspace_width;
  const PixelScalar editor_height = Layout::Scale(40);
  const PixelScalar editor_bottom = padding + editor_height;
  const PixelScalar button_height = Layout::Scale(40);
  constexpr unsigned keyboard_rows = 5;
  const PixelScalar keyboard_top = editor_bottom + padding;
  const PixelScalar keyboard_height = keyboard_rows * button_height;
  const PixelScalar keyboard_bottom = keyboard_top + keyboard_height;

  const bool vertical = client_height >= keyboard_bottom + button_height;

  const PixelScalar button_top = vertical
    ? rc.bottom - button_height
    : keyboard_bottom - button_height;
  const PixelScalar button_bottom = vertical
    ? rc.bottom
    : keyboard_bottom;

  const PixelScalar ok_left = vertical ? 0 : padding;
  const PixelScalar ok_right = vertical
    ? rc.right / 3
    : ok_left + Layout::Scale(80);

  const PixelScalar cancel_left = vertical
    ? ok_right
    : Layout::Scale(175);
  const PixelScalar cancel_right = vertical
    ? rc.right * 2 / 3
    : cancel_left + Layout::Scale(60);

  const PixelScalar clear_left = vertical
    ? cancel_right
    : Layout::Scale(235);
  const PixelScalar clear_right = vertical
    ? rc.right
    : clear_left + Layout::Scale(50);

  WndProperty _editor(client_area, look, _T(""),
                      { 0, padding, backspace_left - padding, editor_bottom },
                      0, WindowStyle());
  _editor.SetReadOnly();
  editor = &_editor;

  WindowStyle button_style;
  button_style.TabStop();

  WndSymbolButton ok_button(client_area, look.button, _T("_X"),
                            { ok_left, button_top, ok_right, button_bottom },
                            button_style, form, mrOK);

  Button cancel_button(client_area, look.button, _("Cancel"),
                          { cancel_left, button_top,
                              cancel_right, button_bottom },
                          button_style, form, mrCancel);

  auto clear_listener = MakeLambdaActionListener([](unsigned id){
      ClearText();
    });
  Button clear_button(client_area, look.button, _("Clear"),
                      { clear_left, button_top,
                          clear_right, button_bottom },
                      button_style, clear_listener, 0);


  KeyboardWidget keyboard(look.button, FormCharacter,
                          show_shift_key, default_shift_state);

  const PixelRect keyboard_rc = {
    padding, keyboard_top,
    rc.right - padding, keyboard_bottom
  };

  keyboard.Initialise(client_area, keyboard_rc);
  keyboard.Prepare(client_area, keyboard_rc);
  keyboard.Show(keyboard_rc);

  kb = &keyboard;

  auto backspace_listener = MakeLambdaActionListener([](unsigned id){
      OnBackspace();
    });
  WndSymbolButton backspace_button(client_area, look.button, _T("_Backspace"),
                                   { backspace_left, padding, rc.right - padding,
                                       editor_bottom },
                                   button_style, backspace_listener, 0);

  AllowedCharactersCallback = accb;

  cursor = 0;
  ClearText();

  if (!StringIsEmpty(text)) {
    CopyString(edittext, text, width);
    cursor = _tcslen(text);
  }

  UpdateTextboxProp();
  bool result = form.ShowModal() == mrOK;

  keyboard.Hide();
  keyboard.Unprepare();

  if (result) {
    CopyString(text, edittext, width);
  }

  return result;
}
