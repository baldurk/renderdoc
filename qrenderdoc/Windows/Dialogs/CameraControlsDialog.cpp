/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "CameraControlsDialog.h"
#include <QKeyEvent>
#include <QMetaEnum>
#include <QMouseEvent>
#include "Code/QRDUtils.h"
#include "ui_CameraControlsDialog.h"

CameraControlsDialog::CameraControlsDialog(ICaptureContext &Ctx, QWidget *parent)
    : QDialog(parent), m_Ctx(Ctx), ui(new Ui::CameraControlsDialog)
{
  ui->setupUi(this);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->nearPlane->setValue(m_Ctx.Config().MeshViewer_CameraNear);
  ui->farPlane->setValue(m_Ctx.Config().MeshViewer_CameraFar);

  m_Keys = m_Ctx.Config().MeshViewer_KeySettings;

  updateDisplayLabels();

  ui->speedMod->setCurrentIndex(0);
  if(m_Ctx.Config().MeshViewer_SpeedModifier == Qt::ShiftModifier)
    ui->speedMod->setCurrentIndex(0);
  else if(m_Ctx.Config().MeshViewer_SpeedModifier == Qt::AltModifier)
    ui->speedMod->setCurrentIndex(1);
  else if(m_Ctx.Config().MeshViewer_SpeedModifier == Qt::ControlModifier)
    ui->speedMod->setCurrentIndex(2);
  else if(m_Ctx.Config().MeshViewer_SpeedModifier == Qt::NoModifier)
    ui->speedMod->setCurrentIndex(3);

  m_Keys.resize((size_t)KeyPressDirection::NumSettings);

  for(QToolButton *b : findChildren<QToolButton *>())
    connect(b, &QToolButton::clicked, this, &CameraControlsDialog::setKey);

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
          &CameraControlsDialog::applyUpdatedControls);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  {
    m_KeybindDialog = new QDialog(this);
    m_KeybindDialog->setWindowTitle(tr("Make Key bind"));
    m_KeybindDialog->setWindowFlags(m_KeybindDialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    m_KeybindDialog->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_KeybindDialog->setFixedSize(200, 80);

    QDialogButtonBox *buttons = new QDialogButtonBox(m_KeybindDialog);
    buttons->addButton(QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::rejected, m_KeybindDialog, &QDialog::reject);

    QLabel *instructions = new QLabel(m_KeybindDialog);
    instructions->setText(tr("Press key or mouse button, or escape to unbind."));
    instructions->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout(m_KeybindDialog);

    layout->addWidget(instructions);
    layout->addWidget(buttons);

    m_KeybindDialog->setLayout(layout);

    m_KeybindDialog->installEventFilter(this);
  }
}

void CameraControlsDialog::applyUpdatedControls()
{
  m_Ctx.Config().MeshViewer_CameraNear = (float)ui->nearPlane->value();
  m_Ctx.Config().MeshViewer_CameraFar = (float)ui->farPlane->value();

  switch(ui->speedMod->currentIndex())
  {
    case 0: m_Ctx.Config().MeshViewer_SpeedModifier = Qt::ShiftModifier; break;
    case 1: m_Ctx.Config().MeshViewer_SpeedModifier = Qt::AltModifier; break;
    case 2: m_Ctx.Config().MeshViewer_SpeedModifier = Qt::ControlModifier; break;
    default: m_Ctx.Config().MeshViewer_SpeedModifier = Qt::NoModifier; break;
  }

  m_Ctx.Config().MeshViewer_KeySettings = m_Keys;

  m_Ctx.Config().Save();
  accept();
}

void CameraControlsDialog::on_resetAll_clicked()
{
  m_Keys.clear();
  m_Keys.resize((size_t)KeyPressDirection::NumSettings);

  updateDisplayLabels();
}

void CameraControlsDialog::setKey()
{
  m_Keybind = 0;
  RDDialog::show(m_KeybindDialog);

  // find the corresponding display label for this button
  QLineEdit *display =
      findChild<QLineEdit *>(QObject::sender()->objectName().replace(lit("Set"), lit("Display")));

  const QLineEdit *labels[(size_t)KeyPressDirection::NumSettings] = {
      // KeyPressDirection::Forward,
      ui->forwardDisplay,
      ui->forwardDisplay_2,
      // KeyPressDirection::Back,
      ui->backwardDisplay,
      ui->backwardDisplay_2,
      // KeyPressDirection::Left,
      ui->leftDisplay,
      ui->leftDisplay_2,
      // KeyPressDirection::Right,
      ui->rightDisplay,
      ui->rightDisplay_2,
      // KeyPressDirection::Up,
      ui->upDisplay,
      ui->upDisplay_2,
      // KeyPressDirection::Down,
      ui->downDisplay,
      ui->downDisplay_2,
  };

  int keyIdx = -1;
  for(int i = 0; i < (int)ARRAY_COUNT(labels); i++)
  {
    if(labels[i] == display)
    {
      keyIdx = i;
      break;
    }
  }

  if(keyIdx < 0)
  {
    qCritical() << "Couldn't identify key being bound";
    return;
  }

  if(m_Keybind == 0)
  {
    // cancelled, do nothing
    return;
  }
  else if(getKeySetting(m_Keybind) == Qt::Key_Escape)
  {
    m_Keys[keyIdx] = makeUnboundSetting();
  }
  else
  {
    bool update = true;

    // check for duplicates
    for(size_t i = 0; i < m_Keys.size(); i++)
    {
      const KeyPressDirection dir = KeyPressDirection(i / 2);
      const bool isPrimary = (i % 2) == 0;

      bool isDefault = false;
      uint32_t k = m_Keys[i];
      if(k == 0)
      {
        k = getDefaultKey(dir, isPrimary);
        isDefault = true;
      }

      if(k == m_Keybind && keyIdx != (int)i)
      {
        QString idxName;
        switch(dir)
        {
          case KeyPressDirection::Left: idxName = tr("Left"); break;
          case KeyPressDirection::Right: idxName = tr("Right"); break;
          case KeyPressDirection::Forward: idxName = tr("Forward"); break;
          case KeyPressDirection::Back: idxName = tr("Back"); break;
          case KeyPressDirection::Up: idxName = tr("Up"); break;
          case KeyPressDirection::Down: idxName = tr("Down"); break;
          default: idxName = lit("???"); break;
        }

        if(isPrimary)
          idxName += tr(" - Primary");
        else
          idxName += tr(" - Secondary");

        if(isDefault)
          idxName += tr(" (Default bind)");

        QMessageBox::StandardButton ret =
            RDDialog::question(this, tr("Conflicting keybind"),
                               tr("%1 is already bound to %2. Continue and unbind old key?")
                                   .arg(nameForSetting(m_Keybind))
                                   .arg(idxName));

        if(ret == QMessageBox::No)
          update = false;
        else if(ret == QMessageBox::Yes)
          m_Keys[i] = makeUnboundSetting();

        break;
      }
    }

    if(update)
      m_Keys[keyIdx] = m_Keybind;
  }

  updateDisplayLabels();
}

bool CameraControlsDialog::eventFilter(QObject *watched, QEvent *event)
{
  if(event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)
  {
    QKeyEvent *key = (QKeyEvent *)event;

    if(key->key() != 0 && key->key() != Qt::Key_unknown && key->key() != Qt::Key_Alt &&
       key->key() != Qt::Key_AltGr && key->key() != Qt::Key_Control && key->key() != Qt::Key_Meta &&
       key->key() != Qt::Key_Shift && key->key() != Qt::Key_CapsLock &&
       key->key() != Qt::Key_NumLock && key->key() != Qt::Key_ScrollLock &&
       key->key() != Qt::Key_Kana_Lock && key->key() != Qt::Key_Kana_Shift &&
       key->key() != Qt::Key_Eisu_Shift && key->key() != Qt::Key_Eisu_toggle)
    {
      m_Keybind = makeKeySetting((Qt::Key)key->key());
      m_KeybindDialog->accept();
      event->accept();
      return true;
    }
  }
  else if(event->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent *mouse = (QMouseEvent *)event;
    if(mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton &&
       mouse->button() != Qt::MiddleButton)
    {
      m_Keybind = makeMouseButtonSetting(mouse->button());
      m_KeybindDialog->accept();
      event->accept();
      return true;
    }
  }
  else if(event->type() == QEvent::Wheel)
  {
    QWheelEvent *mouse = (QWheelEvent *)event;

    m_Keybind = makeMouseWheelSetting(mouse->angleDelta());
    m_KeybindDialog->accept();
    event->accept();
    return true;
  }

  return QObject::eventFilter(watched, event);
}

QString CameraControlsDialog::buttonName(Qt::MouseButton button)
{
  switch(button)
  {
    case Qt::NoButton: return tr("No mouse button");
    case Qt::LeftButton: return tr("Left mouse");
    case Qt::MiddleButton: return tr("Middle mouse");
    case Qt::RightButton: return tr("Right mouse");
    case Qt::BackButton: return tr("Mouse back");
    case Qt::ForwardButton: return tr("Mouse forward");
    case Qt::TaskButton: return tr("Mouse task");
    case Qt::ExtraButton4: return tr("Mouse 7");
    case Qt::ExtraButton5: return tr("Mouse 8");
    case Qt::ExtraButton6: return tr("Mouse 9");
    case Qt::ExtraButton7: return tr("Mouse 10");
    case Qt::ExtraButton8: return tr("Mouse 11");
    case Qt::ExtraButton9: return tr("Mouse 12");
    case Qt::ExtraButton10: return tr("Mouse 13");
    case Qt::ExtraButton11: return tr("Mouse 14");
    case Qt::ExtraButton12: return tr("Mouse 15");
    case Qt::ExtraButton13: return tr("Mouse 16");
    case Qt::ExtraButton14: return tr("Mouse 17");
    case Qt::ExtraButton15: return tr("Mouse 18");
    case Qt::ExtraButton16: return tr("Mouse 19");
    case Qt::ExtraButton17: return tr("Mouse 20");
    case Qt::ExtraButton18: return tr("Mouse 21");
    case Qt::ExtraButton19: return tr("Mouse 22");
    case Qt::ExtraButton20: return tr("Mouse 23");
    case Qt::ExtraButton21: return tr("Mouse 24");
    case Qt::ExtraButton22: return tr("Mouse 25");
    case Qt::ExtraButton23: return tr("Mouse 26");
    case Qt::ExtraButton24: return tr("Mouse 27");
    default: return tr("Unknown button");
  }
}

QString CameraControlsDialog::wheelName(QPoint angleDelta)
{
  if(angleDelta.y() > 0)
    return tr("Mousewheel up");
  else if(angleDelta.y() < 0)
    return tr("Mousewheel down");
  else if(angleDelta.x() < 0)
    return tr("Mousewheel left");
  else if(angleDelta.x() > 0)
    return tr("Mousewheel right");
  return tr("Unknown wheel");
}

QString CameraControlsDialog::nameForSetting(uint32_t k)
{
  if(getKeySetting(k) != Qt::Key_unknown)
    return QKeySequence(getKeySetting(k)).toString();
  else if(getMouseButtonSetting(k) != Qt::MaxMouseButton)
    return buttonName(getMouseButtonSetting(k));
  else if(getMouseWheelSetting(k) != QPoint())
    return wheelName(getMouseWheelSetting(k));

  return tr("Unbound");
}

void CameraControlsDialog::updateDisplayLabels()
{
  QLineEdit *labels[(size_t)KeyPressDirection::NumSettings] = {
      // KeyPressDirection::Forward,
      ui->forwardDisplay,
      ui->forwardDisplay_2,
      // KeyPressDirection::Back,
      ui->backwardDisplay,
      ui->backwardDisplay_2,
      // KeyPressDirection::Left,
      ui->leftDisplay,
      ui->leftDisplay_2,
      // KeyPressDirection::Right,
      ui->rightDisplay,
      ui->rightDisplay_2,
      // KeyPressDirection::Up,
      ui->upDisplay,
      ui->upDisplay_2,
      // KeyPressDirection::Down,
      ui->downDisplay,
      ui->downDisplay_2,
  };

  for(size_t i = 0; i < (size_t)KeyPressDirection::Count; i++)
  {
    for(size_t j = 0; j < 2; j++)
    {
      size_t idx = i * 2 + j;

      if(idx >= m_Keys.size() || m_Keys[idx] == 0)
      {
        labels[idx]->setText(
            tr("Default - %1")
                .arg(nameForSetting(makeKeySetting(getDefaultKey(KeyPressDirection(i), j == 0)))));
      }
      else
      {
        labels[idx]->setText(nameForSetting(m_Keys[idx]));
      }
    }
  }
}

CameraControlsDialog::~CameraControlsDialog()
{
  delete ui;
}

namespace NativeScanCode
{
enum
{
#if defined(Q_OS_WIN32)
  Key_A = 30,
  Key_S = 31,
  Key_D = 32,
  Key_F = 33,
  Key_W = 17,
  Key_R = 19,
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  Key_A = 30 + 8,
  Key_S = 31 + 8,
  Key_D = 32 + 8,
  Key_F = 33 + 8,
  Key_W = 17 + 8,
  Key_R = 19 + 8,
#elif defined(Q_OS_MACOS)
  // scan codes not supported on OS X
  Key_A = 0xDEADBEF1,
  Key_S = 0xDEADBEF2,
  Key_D = 0xDEADBEF3,
  Key_F = 0xDEADBEF4,
  Key_W = 0xDEADBEF5,
  Key_R = 0xDEADBEF6,
#else
#error "Unknown platform! Define NativeScanCode"
#endif
};
};    // namespace NativeScanCode

// default to wasd, this will not work with other keyboard layouts but is the only fallback we have
static Qt::Key defaultPrimaryKeys[(size_t)KeyPressDirection::Count] = {
    Qt::Key_W, Qt::Key_S, Qt::Key_A, Qt::Key_D, Qt::Key_R, Qt::Key_F,
};
// the default secondaries are fortunately fixed keys
static const Qt::Key defaultSecondaryKeys[(size_t)KeyPressDirection::Count] = {
    Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_PageUp, Qt::Key_PageDown,
};
static bool primaryKeysFilled = false;

#if defined(Q_OS_WIN32)
#define NOMINMAX
#include <windows.h>

void FetchDefaultPrimaryKeys()
{
  if(primaryKeysFilled)
    return;

  primaryKeysFilled = true;

  static int scans[(size_t)KeyPressDirection::Count] = {
      NativeScanCode::Key_W, NativeScanCode::Key_S, NativeScanCode::Key_A,
      NativeScanCode::Key_D, NativeScanCode::Key_R, NativeScanCode::Key_F,
  };

  byte state[256] = {};
  for(size_t i = 0; i < (size_t)KeyPressDirection::Count; i++)
  {
    uint vk = MapVirtualKeyW(scans[i], MAPVK_VSC_TO_VK);

    wchar_t buf[8] = {};
    int res = ToUnicode(vk, scans[i], state, buf, 7, 0);

    if(res == 0)
    {
      qCritical() << "couldn't get key for" << i;
    }
    else
    {
      defaultPrimaryKeys[i] = Qt::Key(QChar(buf[0]).toUpper().unicode());
    }
  }
}

#elif defined(Q_OS_LINUX)

#include <dlfcn.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QGuiApplication>
#else
#include <QX11Info>
#endif

// predeclare enough of xkbcommon, so we don't have a new build time dependency on it. Qt will load it for us
extern "C" {
struct xkb_state;
struct xkb_context;
struct xkb_keymap;
typedef uint32_t xkb_keysym_t;
typedef uint32_t xkb_keycode_t;
enum xkb_context_flags
{
  XKB_CONTEXT_NO_DEFAULT_INCLUDES = 1,
};
enum xkb_keymap_compile_flags
{
  XKB_KEYMAP_COMPILE_NO_FLAGS = 0,
};

xkb_context *xkb_context_new(xkb_context_flags flags);
int32_t xkb_x11_get_core_keyboard_device_id(xcb_connection_t *connection);
xkb_keymap *xkb_x11_keymap_new_from_device(xkb_context *context, xcb_connection_t *connection,
                                           int32_t device_id, xkb_keymap_compile_flags flags);
xkb_state *xkb_x11_state_new_from_device(xkb_keymap *keymap, xcb_connection_t *connection,
                                         int32_t device_id);
xkb_keysym_t xkb_state_key_get_one_sym(xkb_state *state, xkb_keycode_t key);
int xkb_keysym_to_utf8(xkb_keysym_t keysym, char *buffer, size_t size);
void xkb_state_unref(xkb_state *state);
void xkb_context_unref(xkb_context *context);
void xkb_keymap_unref(xkb_keymap *keymap);

using PFN_xkb_context_new = decltype(&xkb_context_new);
using PFN_xkb_x11_get_core_keyboard_device_id = decltype(&xkb_x11_get_core_keyboard_device_id);
using PFN_xkb_x11_keymap_new_from_device = decltype(&xkb_x11_keymap_new_from_device);
using PFN_xkb_x11_state_new_from_device = decltype(&xkb_x11_state_new_from_device);
using PFN_xkb_state_key_get_one_sym = decltype(&xkb_state_key_get_one_sym);
using PFN_xkb_keysym_to_utf8 = decltype(&xkb_keysym_to_utf8);
using PFN_xkb_state_unref = decltype(&xkb_state_unref);
using PFN_xkb_context_unref = decltype(&xkb_context_unref);
using PFN_xkb_keymap_unref = decltype(&xkb_keymap_unref);
};

void *findXKBSym(const char *name)
{
  static void *searchLibs[] = {
      RTLD_DEFAULT,
      dlopen("libxkbcommon.so", RTLD_NOW | RTLD_LOCAL),
      dlopen("libxkbcommon-x11.so", RTLD_NOW | RTLD_LOCAL),
      dlopen("libxkbcommon-x11.so.0", RTLD_NOW | RTLD_LOCAL),
  };

  for(void *lib : searchLibs)
  {
    void *ret = dlsym(lib, name);
    if(ret)
      return ret;
  }

  return NULL;
}

void FetchDefaultPrimaryKeys()
{
  if(primaryKeysFilled)
    return;

  primaryKeysFilled = true;

  PFN_xkb_context_new dyn_xkb_context_new = (PFN_xkb_context_new)findXKBSym("xkb_context_new");
  PFN_xkb_x11_get_core_keyboard_device_id dyn_xkb_x11_get_core_keyboard_device_id =
      (PFN_xkb_x11_get_core_keyboard_device_id)findXKBSym("xkb_x11_get_core_keyboard_device_id");
  PFN_xkb_x11_keymap_new_from_device dyn_xkb_x11_keymap_new_from_device =
      (PFN_xkb_x11_keymap_new_from_device)findXKBSym("xkb_x11_keymap_new_from_device");
  PFN_xkb_x11_state_new_from_device dyn_xkb_x11_state_new_from_device =
      (PFN_xkb_x11_state_new_from_device)findXKBSym("xkb_x11_state_new_from_device");
  PFN_xkb_state_key_get_one_sym dyn_xkb_state_key_get_one_sym =
      (PFN_xkb_state_key_get_one_sym)findXKBSym("xkb_state_key_get_one_sym");
  PFN_xkb_keysym_to_utf8 dyn_xkb_keysym_to_utf8 =
      (PFN_xkb_keysym_to_utf8)findXKBSym("xkb_keysym_to_utf8");
  PFN_xkb_state_unref dyn_xkb_state_unref = (PFN_xkb_state_unref)findXKBSym("xkb_state_unref");
  PFN_xkb_context_unref dyn_xkb_context_unref =
      (PFN_xkb_context_unref)findXKBSym("xkb_context_unref");
  PFN_xkb_keymap_unref dyn_xkb_keymap_unref = (PFN_xkb_keymap_unref)findXKBSym("xkb_keymap_unref");

  // Skip X11-specific keyboard mapping in Qt 6
  return;
}

#else

void FetchDefaultPrimaryKeys()
{
  if(primaryKeysFilled)
    return;

  primaryKeysFilled = true;

  qCritical() << "Unsupported platform to fetch default primary keys";
}

#endif

Qt::Key getDefaultKey(KeyPressDirection dir, bool primary)
{
  FetchDefaultPrimaryKeys();

  if(primary)
    return defaultPrimaryKeys[(size_t)dir];

  return defaultSecondaryKeys[(size_t)dir];
}
