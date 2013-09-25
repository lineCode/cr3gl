/*
 * stringresource.h
 *
 *  Created on: Aug 21, 2013
 *      Author: vlopatin
 */

#ifndef STRINGRESOURCE_H_
#define STRINGRESOURCE_H_

#ifndef STRING_RES_IMPL
#define S(x) extern const char * STR_ ## x;
#else
#define S(x) const char * STR_ ## x  = # x;
#endif

S(NOW_READING)
S(BROWSE_FILESYSTEM)
S(BROWSE_LIBRARY)
S(ONLINE_CATALOGS)
S(RECENT_BOOKS)
S(SD_CARD_DIR)
S(INTERNAL_STORAGE_DIR)
S(BOOKS_BY_AUTHOR)
S(BOOKS_BY_TITLE)
S(BOOKS_BY_FILENAME)
S(BOOKS_BY_SERIES)
S(BOOKS_SEARCH)
S(BOOK_COUNT)
S(FOLDER_COUNT)


S(SETTINGS_BROWSER)
S(SETTINGS_BROWSER_DESC)
S(SETTINGS_READER)
S(SETTINGS_READER_DESC)

S(SETTINGS_THEME)
S(SETTINGS_THEME_VALUE_LIGHT)
S(SETTINGS_THEME_VALUE_DARK)
S(SETTINGS_THEME_VALUE_WHITE)
S(SETTINGS_THEME_VALUE_BLACK)

S(SETTINGS_FONTS_AND_COLORS)
S(SETTINGS_FONT_FACE)
S(SETTINGS_FONT_COLOR)

S(SETTINGS_FONT_SAMPLE_TEXT)

S(SETTINGS_FONT_KERNING)
S(SETTINGS_FONT_KERNING_VALUE_ON)
S(SETTINGS_FONT_KERNING_VALUE_OFF)
S(SETTINGS_FONT_ANTIALIASING)
S(SETTINGS_FONT_ANTIALIASING_VALUE_ON)
S(SETTINGS_FONT_ANTIALIASING_VALUE_OFF)
S(SETTINGS_FONT_EMBOLDEN)
S(SETTINGS_FONT_EMBOLDEN_VALUE_ON)
S(SETTINGS_FONT_EMBOLDEN_VALUE_OFF)

#endif /* STRINGRESOURCE_H_ */
