// Copyright 2019 Pokitec
// All rights reserved.

#include "FileView.hpp"
#include "required.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace std;

TEST(FileView, ViewFileForRead) {
    let view = TTauri::FileView("TestFiles/file_view.txt");

    char *test = reinterpret_cast<char *>(view.bytes.data());
    ASSERT_TRUE(strncmp(test, "The quick brown", 15) == 0);
}