// FeatureEditor.cpp
#include "FeatureEditor.h"

#include <QSlider>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

#include <cmath>

static const int kSliderTicks = 1000;

NumericFeatureWidget::NumericFeatureWidget(Camera *camera, const FeatureInfo &info, QWidget *parent)
    : QWidget(parent), m_camera(camera), m_info(info)
{
    const bool isFloat = (info.type == FeatureType::Float);
    const double lo = isFloat ? info.floatMin : (double)info.intMin;
    const double hi = isFloat ? info.floatMax : (double)info.intMax;

    m_spin = new QDoubleSpinBox;
    m_spin->setDecimals(isFloat ? 3 : 0);
    if (hi > lo)
        m_spin->setRange(lo, hi);
    else
        m_spin->setRange(-1e12, 1e12);   // unknown range: leave wide open
    m_spin->setSingleStep(isFloat ? (hi > lo ? (hi - lo) / 100.0 : 1.0)
                                   : (double)info.intInc);
    if (!info.unit.isEmpty())
        m_spin->setSuffix(" " + info.unit);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Slider only makes sense for a bounded range.
    if (hi > lo)
    {
        m_slider = new QSlider(Qt::Horizontal);
        m_slider->setRange(0, kSliderTicks);
        layout->addWidget(m_slider, 1);
        connect(m_slider, &QSlider::valueChanged, this, &NumericFeatureWidget::onSliderMoved);
    }
    layout->addWidget(m_spin);

    connect(m_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &NumericFeatureWidget::onSpinChanged);

    if (!info.writable)
    {
        m_spin->setReadOnly(true);
        m_spin->setEnabled(false);
        if (m_slider) m_slider->setEnabled(false);
    }
    if (!info.tooltip.isEmpty())
        setToolTip(info.tooltip);

    reload();
}

void NumericFeatureWidget::reload()
{
    double v = 0.0;
    bool ok = (m_info.type == FeatureType::Float)
                  ? m_camera->readFloat(m_info.name, &v)
                  : [&] { qint64 i = 0; bool r = m_camera->readInteger(m_info.name, &i); v = (double)i; return r; }();
    if (ok)
        setValueQuiet(v);
}

void NumericFeatureWidget::setValueQuiet(double v)
{
    m_guard = true;
    m_spin->setValue(v);
    if (m_slider)
    {
        double lo = m_spin->minimum(), hi = m_spin->maximum();
        int tick = (hi > lo) ? (int)std::lround((v - lo) / (hi - lo) * kSliderTicks) : 0;
        m_slider->setValue(qBound(0, tick, kSliderTicks));
    }
    m_guard = false;
}

void NumericFeatureWidget::onSpinChanged(double v)
{
    if (m_guard)
        return;
    if (m_info.type == FeatureType::Float)
        m_camera->writeFloat(m_info.name, v);
    else
        m_camera->writeInteger(m_info.name, (qint64)v);
    // Reflect any clamping the camera applied.
    reload();
}

void NumericFeatureWidget::onSliderMoved(int tick)
{
    if (m_guard)
        return;
    double lo = m_spin->minimum(), hi = m_spin->maximum();
    double v = lo + (double)tick / kSliderTicks * (hi - lo);
    m_spin->setValue(v);   // triggers onSpinChanged -> writes
}

QWidget *makeFeatureEditor(Camera *camera, const FeatureInfo &info, QWidget *parent)
{
    switch (info.type)
    {
    case FeatureType::Integer:
    case FeatureType::Float:
        return new NumericFeatureWidget(camera, info, parent);

    case FeatureType::Enumeration:
    {
        auto *combo = new QComboBox(parent);
        combo->addItems(info.enumEntries);
        QString cur;
        if (camera->readEnumeration(info.name, &cur))
            combo->setCurrentText(cur);
        combo->setEnabled(info.writable);
        if (!info.tooltip.isEmpty()) combo->setToolTip(info.tooltip);
        QObject::connect(combo, &QComboBox::currentTextChanged,
                         combo, [camera, info](const QString &text) {
                             camera->writeEnumeration(info.name, text);
                         });
        return combo;
    }

    case FeatureType::Boolean:
    {
        auto *chk = new QCheckBox(parent);
        bool b = false;
        if (camera->readBoolean(info.name, &b))
            chk->setChecked(b);
        chk->setEnabled(info.writable);
        if (!info.tooltip.isEmpty()) chk->setToolTip(info.tooltip);
        QObject::connect(chk, &QCheckBox::toggled, chk, [camera, info](bool on) {
            camera->writeBoolean(info.name, on);
        });
        return chk;
    }

    case FeatureType::Command:
    {
        auto *btn = new QPushButton("Execute", parent);
        btn->setEnabled(info.writable);
        if (!info.tooltip.isEmpty()) btn->setToolTip(info.tooltip);
        QObject::connect(btn, &QPushButton::clicked, btn, [camera, info]() {
            camera->executeCommand(info.name);
        });
        return btn;
    }

    case FeatureType::String:
    {
        auto *edit = new QLineEdit(parent);
        QString s;
        if (camera->readString(info.name, &s))
            edit->setText(s);
        edit->setReadOnly(!info.writable);
        if (!info.tooltip.isEmpty()) edit->setToolTip(info.tooltip);
        QObject::connect(edit, &QLineEdit::editingFinished, edit, [camera, info, edit]() {
            camera->writeString(info.name, edit->text());
        });
        return edit;
    }

    default:
        return new QLabel("<unsupported>", parent);
    }
}
