// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_layout.h"
#include "citra_qt/ui_settings.h"
#include "ui_configure_layout.h"

#include "core/settings.h"

ConfigureLayout::ConfigureLayout(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureLayout)
{
    ui->setupUi(this);
    this->setConfiguration();
}

ConfigureLayout::~ConfigureLayout()
{
    delete ui;
}

void ConfigureLayout::setConfiguration() {
    ui->layout_combobox->setCurrentIndex(static_cast<int>(Settings::values.layout_option));
    ui->swap_screen->setChecked(Settings::values.swap_screen);
}

void ConfigureLayout::applyConfiguration() {
    Settings::values.layout_option = static_cast<Settings::LayoutOption>(ui->layout_combobox->currentIndex());
    Settings::values.swap_screen = ui->swap_screen->isChecked();
}
