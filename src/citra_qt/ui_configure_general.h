/********************************************************************************
** Form generated from reading UI file 'configure_general.ui'
**
** Created by: Qt User Interface Compiler version 5.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONFIGURE_GENERAL_H
#define UI_CONFIGURE_GENERAL_H

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
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "hotkeys.h"

QT_BEGIN_NAMESPACE

class Ui_ConfigureGeneral
{
public:
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox;
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout_2;
    QCheckBox *toggle_deepscan;
    QCheckBox *toggle_check_exit;
    QGroupBox *groupBox_2;
    QHBoxLayout *horizontalLayout_7;
    QVBoxLayout *verticalLayout_5;
    QCheckBox *toggle_cpu_jit;
    QGroupBox *groupBox_4;
    QHBoxLayout *horizontalLayout_5;
    QVBoxLayout *verticalLayout_6;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label;
    QComboBox *region_combobox;
    QGroupBox *groupBox_3;
    QHBoxLayout *horizontalLayout_4;
    QVBoxLayout *verticalLayout_4;
    GHotkeysDialog *widget;

    void setupUi(QWidget *ConfigureGeneral)
    {
        if (ConfigureGeneral->objectName().isEmpty())
            ConfigureGeneral->setObjectName(QStringLiteral("ConfigureGeneral"));
        ConfigureGeneral->resize(300, 377);
        horizontalLayout = new QHBoxLayout(ConfigureGeneral);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        groupBox = new QGroupBox(ConfigureGeneral);
        groupBox->setObjectName(QStringLiteral("groupBox"));
        horizontalLayout_3 = new QHBoxLayout(groupBox);
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        toggle_deepscan = new QCheckBox(groupBox);
        toggle_deepscan->setObjectName(QStringLiteral("toggle_deepscan"));

        verticalLayout_2->addWidget(toggle_deepscan);

        toggle_check_exit = new QCheckBox(groupBox);
        toggle_check_exit->setObjectName(QStringLiteral("toggle_check_exit"));

        verticalLayout_2->addWidget(toggle_check_exit);


        horizontalLayout_3->addLayout(verticalLayout_2);


        verticalLayout->addWidget(groupBox);

        groupBox_2 = new QGroupBox(ConfigureGeneral);
        groupBox_2->setObjectName(QStringLiteral("groupBox_2"));
        horizontalLayout_7 = new QHBoxLayout(groupBox_2);
        horizontalLayout_7->setObjectName(QStringLiteral("horizontalLayout_7"));
        verticalLayout_5 = new QVBoxLayout();
        verticalLayout_5->setObjectName(QStringLiteral("verticalLayout_5"));
        toggle_cpu_jit = new QCheckBox(groupBox_2);
        toggle_cpu_jit->setObjectName(QStringLiteral("toggle_cpu_jit"));

        verticalLayout_5->addWidget(toggle_cpu_jit);


        horizontalLayout_7->addLayout(verticalLayout_5);


        verticalLayout->addWidget(groupBox_2);

        groupBox_4 = new QGroupBox(ConfigureGeneral);
        groupBox_4->setObjectName(QStringLiteral("groupBox_4"));
        horizontalLayout_5 = new QHBoxLayout(groupBox_4);
        horizontalLayout_5->setObjectName(QStringLiteral("horizontalLayout_5"));
        verticalLayout_6 = new QVBoxLayout();
        verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));
        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName(QStringLiteral("horizontalLayout_6"));
        label = new QLabel(groupBox_4);
        label->setObjectName(QStringLiteral("label"));

        horizontalLayout_6->addWidget(label);

        region_combobox = new QComboBox(groupBox_4);
        region_combobox->insertItems(0, QStringList()
         << QStringLiteral("JPN")
         << QStringLiteral("USA")
         << QStringLiteral("EUR")
         << QStringLiteral("AUS")
         << QStringLiteral("CHN")
         << QStringLiteral("KOR")
         << QStringLiteral("TWN")
        );
        region_combobox->setObjectName(QStringLiteral("region_combobox"));

        horizontalLayout_6->addWidget(region_combobox);


        verticalLayout_6->addLayout(horizontalLayout_6);


        horizontalLayout_5->addLayout(verticalLayout_6);


        verticalLayout->addWidget(groupBox_4);

        groupBox_3 = new QGroupBox(ConfigureGeneral);
        groupBox_3->setObjectName(QStringLiteral("groupBox_3"));
        horizontalLayout_4 = new QHBoxLayout(groupBox_3);
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
        widget = new GHotkeysDialog(groupBox_3);
        widget->setObjectName(QStringLiteral("widget"));

        verticalLayout_4->addWidget(widget);


        horizontalLayout_4->addLayout(verticalLayout_4);


        verticalLayout->addWidget(groupBox_3);


        horizontalLayout->addLayout(verticalLayout);


        retranslateUi(ConfigureGeneral);

        QMetaObject::connectSlotsByName(ConfigureGeneral);
    } // setupUi

    void retranslateUi(QWidget *ConfigureGeneral)
    {
        ConfigureGeneral->setWindowTitle(QApplication::translate("ConfigureGeneral", "Form", 0));
        groupBox->setTitle(QApplication::translate("ConfigureGeneral", "General", 0));
        toggle_deepscan->setText(QApplication::translate("ConfigureGeneral", "Recursive scan for game folder", 0));
        toggle_check_exit->setText(QApplication::translate("ConfigureGeneral", "Confirm exit while emulation is running", 0));
        groupBox_2->setTitle(QApplication::translate("ConfigureGeneral", "Performance", 0));
        toggle_cpu_jit->setText(QApplication::translate("ConfigureGeneral", "Enable CPU JIT", 0));
        groupBox_4->setTitle(QApplication::translate("ConfigureGeneral", "Emulation", 0));
        label->setText(QApplication::translate("ConfigureGeneral", "Region:", 0));
        groupBox_3->setTitle(QApplication::translate("ConfigureGeneral", "Hotkeys", 0));
    } // retranslateUi

};

namespace Ui {
    class ConfigureGeneral: public Ui_ConfigureGeneral {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONFIGURE_GENERAL_H
