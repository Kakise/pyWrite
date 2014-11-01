#include "editortask.h"

#include <cstdio>
#include <libndls.h>
#include <nucleus.h>

#include "font.h"
#include "texturetools.h"
#include "browsertask.h"
#include "cursortask.h"
#include "dialogtask.h"

EditorTask editor_task;

extern "C" char nio_ascii_get(int *a);

constexpr char KEY_UP = 0xF0, KEY_DOWN = 0xF1, KEY_LEFT = 0xF2, KEY_RIGHT = 0xF3, KEY_LEFT_SHIFT = 0xF4, KEY_RIGHT_SHIFT = 0xF5;

EditorTask::EditorTask()
{
    float menuitem_width = 0;
    for(auto &item : menu_items)
    {
        item.width = fontWidth(item.name, EFont::Normal);
        menuitem_width += item.width;
    }

    float padding = menu_width / menuitem_width;
    for(auto &item : menu_items)
        item.width = padding * item.width;
}

void EditorTask::makeCurrent()
{
    Task::makeCurrent();

    //Did we come back from the file browser?
    if(requested_open)
    {
        requested_open = false;

        //User canceled
        if(browser_task.filepath == "")
            return;

        //Load file
        filepath = browser_task.filepath;
        FILE *f = fopen(browser_task.filepath.c_str(), "r");
        if(f)
        {
            fseek(f, 0, SEEK_END);
            buffer.resize(ftell(f));
            fseek(f, 0, SEEK_SET);
            fread(reinterpret_cast<void*>(const_cast<char*>(buffer.data())), 1, buffer.size(), f);
            fclose(f);

            //Reset cursor and view
            sel_start = sel_line = x_offset = line_top = 0;
        }
        else
            dialog_task.showMessage("Could not open '" + filepath.substr(browser_task.filepath.find_last_of('/') + 1, std::string::npos) + "'!");
    }
    else if(requested_save)
    {
        requested_save = false;

        //User canceled
        if(browser_task.filepath == "")
            return;

        filepath = browser_task.filepath;

        FILE *f = fopen(filepath.c_str(), "r");
        if(f)
        {
            fclose(f);

            if(!requested_confirm)
            {
                dialog_task.confirm("Overwrite existing file '" + filepath.substr(browser_task.filepath.find_last_of('/') + 1, std::string::npos) + "'?");

                requested_save = true;
                requested_confirm = true;
                return;
            }
            else if(!dialog_task.confirmed)
                return;
        }

        f = fopen(filepath.c_str(), "wb");
        if(f)
        {
            fwrite(buffer.c_str(), buffer.length(), 1, f);
            fclose(f);
        }
        else
            dialog_task.showMessage("Could not save '" + browser_task.filepath.substr(browser_task.filepath.find_last_of('/') + 1, std::string::npos) + "'!");
    }
}

void EditorTask::logic()
{
    int i;
    char c = nio_ascii_get(&i);
    if(isKeyPressed(KEY_NSPIRE_UP))
        c = KEY_UP;
    else if(isKeyPressed(KEY_NSPIRE_DOWN))
        c = KEY_DOWN;
    else if(isKeyPressed(KEY_NSPIRE_LEFT))
        c = isKeyPressed(KEY_NSPIRE_SHIFT) ? KEY_LEFT_SHIFT : KEY_LEFT;
    else if(isKeyPressed(KEY_NSPIRE_RIGHT))
        c = isKeyPressed(KEY_NSPIRE_SHIFT) ? KEY_RIGHT_SHIFT : KEY_RIGHT;

    if(isKeyPressed(KEY_NSPIRE_ESC) && !key_hold_down)
    {
        running = false;

        key_hold_down = true;
    }
    else if(c > 1)
    {
        const char *current_sel; int x = 0, lines = (240 - 4 - fontHeight()) / fontHeight();

        //Key repeat handling
        if(key_hold_down)
        {
            key_repeat++;
            if(key_repeat >= key_repeat_count)
                key_repeat = key_repeat_count - key_repeat_speed;
            else
                goto skip_input;
        }

        switch(c)
        {
        case KEY_LEFT_SHIFT:
            if(x_offset >= SCREEN_WIDTH/2)
                x_offset -= SCREEN_WIDTH/2;
            break;
        case KEY_RIGHT_SHIFT:
            x_offset += SCREEN_WIDTH/2;
            break;

        case KEY_UP:
            if(sel_line == 0)
                break;
            //Fall through

        case KEY_DOWN:
            //Find beginning of line
            current_sel = atLine(sel_line);

            //Get x position of selection
            while(static_cast<unsigned int>(current_sel - buffer.c_str()) < sel_start)
                x += fontWidth(*current_sel++);

            //Get new line and find char at X position
            current_sel = atLine(c == KEY_UP ? sel_line - 1 : sel_line + 1);
            while(*current_sel && *current_sel != '\n' && x > 0)
                x -= fontWidth(*current_sel++);

            sel_line = linesUntil(current_sel);
            sel_start = current_sel - buffer.c_str();

            if(sel_line < line_top)
                --line_top;
            else if(sel_line >= line_top + lines)
                ++line_top;

            updateCursor();
            break;

        case KEY_RIGHT:
            if(sel_start < buffer.length())
            {
                if(*(buffer.c_str() + sel_start) == '\n')
                    ++sel_line;

                ++sel_start;
                updateCursor();
            }
            break;
        case KEY_LEFT:
            if(sel_start > 0)
            {
                --sel_start;

                if(*(buffer.c_str() + sel_start) == '\n')
                    --sel_line;

                updateCursor();
            }
            break;

        case '\b':
            if(sel_start > 0)
            {
                buffer.erase(buffer.begin() + (sel_start - 1));
                --sel_start;
            }
            break;

        case '\n':
            buffer.insert(buffer.begin() + sel_start, c);
            ++sel_start;
            sel_line = linesUntil(buffer.c_str() + sel_start);
            break;

        default:
            buffer.insert(buffer.begin() + sel_start, c);
            ++sel_start;
            break;
        }

        skip_input:

        key_hold_down = true;
    }
    else
    {
        key_hold_down = false;
        key_repeat = 0;
    }

    //Handle menu
    if(cursor_task.state && cursor_task.y < menu_height && !key_hold_down)
    {
        unsigned int x = cursor_task.x - menu_x;
        for(auto &item : menu_items)
        {
            //If cursor on the current menu item
            if(x < item.width)
            {
                (this->*(item.action))();
                break;
            }

            x -= item.width;
        }

        key_hold_down = true;
    }

    //Cursor blinking
    if(cursor_tick == 2 * cursor_time)
        cursor_tick = 0;
    else
        ++cursor_tick;
}

void EditorTask::render()
{
    //Draw menu
    drawRectangle(*screen, menu_x, 0, menu_width, menu_height, 0x0000);
    unsigned int item_x = menu_x;
    for(auto &item : menu_items)
    {
        if(cursor_task.y < menu_height && cursor_task.x > item_x && cursor_task.x < item_x + item.width)
            drawStringCenter(item.name, 0x0000, *screen, item_x + item.width/2, 0, EFont::Large);
        else
            drawStringCenter(item.name, 0x0000, *screen, item_x + item.width/2, 4, EFont::Normal);

        item_x += item.width;
    }

    const char *ptr = atLine(line_top);
    int x = 4 - x_offset; unsigned int y = menu_height + 4;
    const int start_x = x;
    const unsigned int font_height = fontHeight();
    while(y + font_height < SCREEN_HEIGHT)
    {
        //Draw '|' as cursor
        if((unsigned int)(ptr - buffer.c_str()) == sel_start && cursor_tick > cursor_time && x >= 3)
            drawChar('|', 0x0000, *screen, x - 3, y);

        //If mouse clicked and on current char set selection
        if(cursor_task.state && cursor_task.y >= y && cursor_task.y < y + font_height
                && static_cast<int>(cursor_task.x) >= x && static_cast<int>(cursor_task.x) < static_cast<int>(x + fontWidth(*ptr)))
        {
            sel_start = ptr - buffer.c_str();
            sel_line = linesUntil(ptr);

            //Show cursor
            cursor_tick = cursor_time;
        }

        if(!*ptr)
            break;

        if(*ptr == '\n')
        {
            x = start_x;
            y += font_height;
        }
        else if(*ptr && x + fontWidth(*ptr) < SCREEN_WIDTH && x >= 0)
            x += drawChar(*ptr, 0x0000, *screen, x, y);
        else if(*ptr)
            x += fontWidth(*ptr);

        ++ptr;
    }
}

void EditorTask::menuNew()
{
    filepath = "";
    buffer = "";

    //Reset cursor and view
    sel_start = sel_line = x_offset = line_top = 0;
}

void EditorTask::menuOpen()
{
    requested_open = true;
    browser_task.requestOpen();
}

void EditorTask::menuSave()
{
    if(filepath == "")
    {
        menuSaveAs();
        return;
    }

    FILE *f = fopen(filepath.c_str(), "wb");
    if(f)
    {
        fwrite(buffer.c_str(), buffer.length(), 1, f);
        fclose(f);
    }
    else
        dialog_task.showMessage("Could not save '" + browser_task.filepath.substr(browser_task.filepath.find_last_of('/') + 1, std::string::npos) + "'!");
}

void EditorTask::menuSaveAs()
{
    requested_save = !(requested_confirm = false);
    browser_task.requestSave();
}

void EditorTask::menuRun()
{
    if(filepath == "")
    {
        dialog_task.showMessage("You have to save the file first!");
        return;
    }

    const char* argv[] = {filepath.c_str()};
    key_hold_down = true;
    nl_exec("/documents/ndless/micropython.tns", sizeof(argv), const_cast<char**>(argv));
}

void EditorTask::menuExit()
{
    running = false;
}

const char *EditorTask::atLine(unsigned int l)
{
    const char *ptr = buffer.c_str();
    while(l--)
        while(*ptr && *ptr++ != '\n');

    return ptr;
}

unsigned int EditorTask::linesUntil(const char *end)
{
    unsigned int ret = 0;
    const char *ptr = buffer.c_str();
    while(ptr != end && *ptr)
        if(*ptr++ == '\n')
            ++ret;

    return ret;
}

//Update horiztontal scroll offset according to cursor position
void EditorTask::updateCursor()
{
    int cursor_x = 0;
    const char *ptr = atLine(sel_line);
    while(ptr < buffer.c_str() + sel_start)
        cursor_x += fontWidth(*ptr++);

    cursor_x -= x_offset;

    while(cursor_x >= static_cast<int>(SCREEN_WIDTH - fontWidth('M')))
    {
        cursor_x -= SCREEN_WIDTH/2;
        x_offset += SCREEN_WIDTH/2;
    }

    while(cursor_x <= 3 && x_offset >= SCREEN_WIDTH/2)
    {
        cursor_x += SCREEN_WIDTH/2;
        x_offset -= SCREEN_WIDTH/2;
    }

    //Show cursor
    cursor_tick = cursor_time;
}
