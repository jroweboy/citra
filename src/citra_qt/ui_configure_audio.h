/********************************************************************************
** Form generated from reading UI file 'configure_audio.ui'
**
** Created by: Qt User Interface Compiler version 5.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONFIGURE_AUDIO_H
#define UI_CONFIGURE_AUDIO_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ConfigureAudio
{
public:
    QVBoxLayout *vboxLayout;
    QGroupBox *groupBox;
    QVBoxLayout *vboxLayout1;
    QHBoxLayout *hboxLayout;
    QLabel *label;
    QComboBox *output_sink_combo_box;
    QCheckBox *toggle_audio_stretching;
    QHBoxLayout *hboxLayout1;
    QLabel *label1;
    QComboBox *audio_device_combo_box;
    QSpacerItem *spacerItem;

    void setupUi(QWidget *ConfigureAudio)
    {
        if (ConfigureAudio->objectName().isEmpty())
            ConfigureAudio->setObjectName(QStringLiteral("ConfigureAudio"));
        vboxLayout = new QVBoxLayout(ConfigureAudio);
        vboxLayout->setObjectName(QStringLiteral("vboxLayout"));
        groupBox = new QGroupBox(ConfigureAudio);
        groupBox->setObjectName(QStringLiteral("groupBox"));
        vboxLayout1 = new QVBoxLayout(groupBox);
        vboxLayout1->setObjectName(QStringLiteral("vboxLayout1"));
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName(QStringLiteral("hboxLayout"));
        label = new QLabel(groupBox);
        label->setObjectName(QStringLiteral("label"));

        hboxLayout->addWidget(label);

        output_sink_combo_box = new QComboBox(groupBox);
        output_sink_combo_box->setObjectName(QStringLiteral("output_sink_combo_box"));

        hboxLayout->addWidget(output_sink_combo_box);


        vboxLayout1->addLayout(hboxLayout);

        toggle_audio_stretching = new QCheckBox(groupBox);
        toggle_audio_stretching->setObjectName(QStringLiteral("toggle_audio_stretching"));

        vboxLayout1->addWidget(toggle_audio_stretching);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName(QStringLiteral("hboxLayout1"));
        label1 = new QLabel(groupBox);
        label1->setObjectName(QStringLiteral("label1"));

        hboxLayout1->addWidget(label1);

        audio_device_combo_box = new QComboBox(groupBox);
        audio_device_combo_box->setObjectName(QStringLiteral("audio_device_combo_box"));

        hboxLayout1->addWidget(audio_device_combo_box);


        vboxLayout1->addLayout(hboxLayout1);


        vboxLayout->addWidget(groupBox);

        spacerItem = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        vboxLayout->addItem(spacerItem);


        retranslateUi(ConfigureAudio);

        QMetaObject::connectSlotsByName(ConfigureAudio);
    } // setupUi

    void retranslateUi(QWidget *ConfigureAudio)
    {
        groupBox->setTitle(QApplication::translate("ConfigureAudio", "Audio", 0));
        label->setText(QApplication::translate("ConfigureAudio", "Output Engine:", 0));
        toggle_audio_stretching->setText(QApplication::translate("ConfigureAudio", "Enable audio stretching", 0));
#ifndef QT_NO_TOOLTIP
        toggle_audio_stretching->setToolTip(QApplication::translate("ConfigureAudio", "This post-processing effect adjusts audio speed to match emulation speed and helps prevent audio stutter. This however increases audio latency.", 0));
#endif // QT_NO_TOOLTIP
        label1->setText(QApplication::translate("ConfigureAudio", "Audio Device:", 0));
        Q_UNUSED(ConfigureAudio);
    } // retranslateUi

};

namespace Ui {
    class ConfigureAudio: public Ui_ConfigureAudio {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONFIGURE_AUDIO_H
