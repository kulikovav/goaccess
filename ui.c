/** 
 * ui.c -- curses user interface
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.2
 * Last Modified: Sunday, July 25, 2010
 * Path:  /ui.c
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * GoAccess is released under the GNU/GPL License.
 * Copy of the GNU General Public License is attached to this source 
 * distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

/* "_XOPEN_SOURCE" is required for the GNU libc to export "strptime(3)"
 * correctly. */
#define _XOPEN_SOURCE 700
#define STDIN_FILENO  0

#include <string.h>
#include <curses.h>
#include <time.h>
#include <menu.h>
#include <glib.h>
#include <stdlib.h>
#include <GeoIP.h>

#include "parser.h"
#include "alloc.h"
#include "commons.h"
#include "util.h"
#include "ui.h"

static MENU *my_menu = NULL;
static ITEM **items = NULL;

/* creation - ncurses' window handling */
WINDOW *
create_win (WINDOW * main_win)
{
    int y, x;
    getmaxyx (main_win, y, x);
    return (newwin (y - 12, x - 40, 8, 20));
}

/* deletion - ncurses' window handling */
void
close_win (WINDOW * w)
{
    if (w == NULL)
        return;
    wclear (w);
    wrefresh (w);
    delwin (w);
}

/* get the current date-time */
void
generate_time (void)
{
    now = time (NULL);
    now_tm = localtime (&now);
}

void
draw_header (WINDOW * win, char *header, int x, int y, int w, int color)
{
    init_pair (1, COLOR_BLACK, COLOR_GREEN);
    init_pair (2, COLOR_BLACK, COLOR_CYAN);

    wattron (win, COLOR_PAIR (color));
    mvwhline (win, y, x, ' ', w);
    mvwaddnstr (win, y, x, header, w);
    wattroff (win, COLOR_PAIR (color));
}

void
update_header (WINDOW * header_win, int current)
{
    int row = 0, col = 0;

    getmaxyx (stdscr, row, col);
    wattron (header_win, COLOR_PAIR (BLUE_GREEN));
    wmove (header_win, 0, 30);
    mvwprintw (header_win, 0, col - 20, "[Active Module %d]", current);
    wattroff (header_win, COLOR_PAIR (BLUE_GREEN));
    wrefresh (header_win);
}

void
term_size (WINDOW * main_win)
{
    getmaxyx (stdscr, term_h, term_w);

    real_size_y = term_h - (MAX_HEIGHT_HEADER + MAX_HEIGHT_FOOTER);
    wresize (main_win, real_size_y, term_w);
    wmove (main_win, real_size_y, 0);
}

void
display_general (WINDOW * header_win, struct logger *logger, char *ifile)
{
    int row, col;
    getmaxyx (stdscr, row, col);
    draw_header (header_win,
                 " General Statistics - Information analyzed from log file - Unique totals",
                 0, 0, col, 1);

    /* general stats */
    wattron (header_win, COLOR_PAIR (COL_CYAN));
    wattron (header_win, A_BOLD);
    mvwprintw (header_win, 2, 18, "%u", logger->total_process);
    mvwprintw (header_win, 3, 18, "%u", logger->total_invalid);
    mvwprintw (header_win, 4, 18, "%d sec", (int) end_proc - start_proc);
    mvwprintw (header_win, 2, 50, "%d",
               g_hash_table_size (ht_unique_visitors));
    mvwprintw (header_win, 3, 50, "%d", g_hash_table_size (ht_requests));
    mvwprintw (header_win, 4, 50, "%d",
               g_hash_table_size (ht_requests_static));
    mvwprintw (header_win, 2, 74, "%d", g_hash_table_size (ht_referers));
    mvwprintw (header_win, 3, 74, "%d", g_hash_table_size (ht_keyphrases));
    double log_size = ((double) file_size (ifile)) / MB;
    mvwprintw (header_win, 2, 86, "%.2fMB", log_size);

    double tot_bw = (float) req_size / GB;
    if (bandwidth_flag)
        mvwprintw (header_win, 3, 86, "%.3f GB", tot_bw);
    else
        mvwprintw (header_win, 3, 86, "N/A");

    wattroff (header_win, A_BOLD);
    wattroff (header_win, COLOR_PAIR (COL_CYAN));
    wattron (header_win, COLOR_PAIR (COL_YELLOW));
    mvwprintw (header_win, 4, 58, "%s", ifile);
    wattroff (header_win, COLOR_PAIR (COL_YELLOW));

    /*labels */
    wattron (header_win, COLOR_PAIR (COL_WHITE));
    mvwprintw (header_win, 2, 2, "Total hits");
    mvwprintw (header_win, 3, 2, "Invalid entries");
    mvwprintw (header_win, 4, 2, "Generation Time");
    mvwprintw (header_win, 2, 28, "Total Unique Visitors");
    mvwprintw (header_win, 3, 28, "Total Requests");
    mvwprintw (header_win, 4, 28, "Total Static Requests");
    mvwprintw (header_win, 2, 58, "Total Referrers");
    mvwprintw (header_win, 3, 58, "Total 404");
    mvwprintw (header_win, 3, 82, "BW");
    mvwprintw (header_win, 2, 82, "Log");
    wattroff (header_win, COLOR_PAIR (COL_WHITE));
}

void
create_graphs (WINDOW * main_win, struct struct_display **s_display,
               struct logger *logger, int i, int module, int max)
{
    int x, y, xx, r, col, row;
    float l_bar, scr_cal, orig_cal;
    struct tm tm;
    /* max possible size of date */
    char buf[12] = "";
    GHashTable *hash_table = NULL;

    memset (&tm, 0, sizeof (tm));

    getyx (main_win, y, x);
    getmaxyx (stdscr, row, col);

    switch (module) {
     case UNIQUE_VISITORS:
         hash_table = ht_unique_visitors;
         break;
     case REQUESTS:
         hash_table = ht_requests;
         break;
     case REQUESTS_STATIC:
         hash_table = ht_requests_static;
         break;
     case REFERRERS:
         hash_table = ht_referers;
         break;
     case NOT_FOUND:
         hash_table = ht_not_found_requests;
         break;
     case OS:
         hash_table = ht_os;
         break;
     case BROWSERS:
         hash_table = ht_browsers;
         break;
     case HOSTS:
         hash_table = ht_hosts;
         break;
     case STATUS_CODES:
         hash_table = ht_status_code;
         break;
     case REFERRING_SITES:
         hash_table = ht_referring_sites;
         break;
     case KEYPHRASES:
         hash_table = ht_keyphrases;
         break;
    }

    orig_cal = (float) (s_display[i]->hits * 100);
    l_bar = (float) (s_display[i]->hits * 100);

    if (module != 8 && module != 9)
        orig_cal = (l_bar / g_hash_table_size (hash_table));
    else
        orig_cal = (orig_cal / logger->total_process);

    l_bar = (l_bar / max);
    if (s_display[i]->module == 1) {
        strptime (s_display[i]->data, "%Y%m%d", &tm);
        strftime (buf, sizeof (buf), "%d/%b/%Y", &tm);
        mvwprintw (main_win, y, 18, "%s", buf);
    } else {
        mvwprintw (main_win, y, 18, "%s", s_display[i]->data);
        /* get http status code */
        if (s_display[i]->module == 9) {
            mvwprintw (main_win, y, 23, "%s",
                       verify_status_code (s_display[i]->data));
        }
    }
    mvwprintw (main_win, y, 2, "%d", s_display[i]->hits);

    if (s_display[i]->hits == max)
        wattron (main_win, COLOR_PAIR (COL_YELLOW));
    else
        wattron (main_win, COLOR_PAIR (COL_RED));

    mvwprintw (main_win, y, 10, "%4.2f%%", orig_cal);

    if (s_display[i]->hits == max)
        wattroff (main_win, COLOR_PAIR (COL_YELLOW));
    else
        wattroff (main_win, COLOR_PAIR (COL_RED));

    if (s_display[i]->module == 9)
        return;

    scr_cal = (float) ((col - 38));
    scr_cal = (float) scr_cal / 100;
    l_bar = l_bar * scr_cal;

    for (r = 0, xx = 35; r < (int) l_bar; r++, xx++) {
        wattron (main_win, COLOR_PAIR (COL_GREEN));
        mvwprintw (main_win, y, xx, "|");
        wattroff (main_win, COLOR_PAIR (COL_GREEN));
    }
}

int
get_max_value (struct struct_display **s_display, struct logger *logger,
               int module)
{
    int i, temp = 0;
    for (i = 0; i < logger->alloc_counter; i++) {
        if (s_display[i]->module == module) {
            if (s_display[i]->hits > temp)
                temp = s_display[i]->hits;
        }
    }
    return temp;
}

/* ###NOTE: Modules 6, 7 are based on module 1 totals 
   this way we avoid the overhead of adding them up */
void
display_content (WINDOW * main_win, struct struct_display **s_display,
                 struct logger *logger, struct scrolling scrolling)
{
    int i, x, y, max = 0, until = 0, start = 0, pos_y = 0;

    getmaxyx (stdscr, term_h, term_w);
    getmaxyx (main_win, y, x);

    if (term_h < MIN_HEIGHT || term_w < MIN_WIDTH)
        error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                       "Minimum screen size - 97 columns by 40 lines");

    if (logger->alloc_counter > real_size_y)
        until = real_size_y + scrolling.init_scrl_main_win;
    else
        logger->alloc_counter;

    start = scrolling.init_scrl_main_win;

    /* making sure we dont go over logger->alloc_counter */
    if (until > logger->alloc_counter)
        until = logger->alloc_counter;

    for (i = start; i < until; i++, pos_y++) {
        if (s_display[i]->hits != 0)
            mvwprintw (main_win, pos_y, 2, "%d", s_display[i]->hits);
        /* draw headers */
        if ((i % 10) == 0)
            draw_header (main_win, s_display[i]->data, 0, pos_y, x, 1);
        else if ((i % 10) == 1) {
            draw_header (main_win, s_display[i]->data, 0, pos_y, x, 2);
        } else if (((s_display[i]->module == UNIQUE_VISITORS))
                   && ((i % 10 >= 3) && (i % 10 <= 8)
                       && (s_display[i]->hits != 0))) {
            max = get_max_value (s_display, logger, UNIQUE_VISITORS);
            create_graphs (main_win, s_display, logger, i, 1, max);

        } else if (((s_display[i]->module == OS))
                   && ((i % 10 >= 3) && (i % 10 <= 8)
                       && (s_display[i]->hits != 0))) {
            max = get_max_value (s_display, logger, OS);
            create_graphs (main_win, s_display, logger, i, 1, max);

        } else if (((s_display[i]->module == BROWSERS))
                   && ((i % 10 >= 3) && (i % 10 <= 8)
                       && (s_display[i]->hits != 0))) {
            max = get_max_value (s_display, logger, BROWSERS);
            create_graphs (main_win, s_display, logger, i, 1, max);

        } else if (((s_display[i]->module == HOSTS))
                   && ((i % 10 >= 3) && (i % 10 <= 8)
                       && (s_display[i]->hits != 0))) {
            max = get_max_value (s_display, logger, HOSTS);
            create_graphs (main_win, s_display, logger, i, HOSTS, max);

        } else if (((s_display[i]->module == STATUS_CODES))
                   && ((i % 10 >= 3) && (i % 10 <= 8)
                       && (s_display[i]->hits != 0))) {
            max = get_max_value (s_display, logger, STATUS_CODES);
            create_graphs (main_win, s_display, logger, i, STATUS_CODES, max);

        } else
            mvwprintw (main_win, pos_y, 10, "%s", s_display[i]->data);
    }
}

/* ###NOTE: Modules 6, 7 are based on module 1 totals 
   this way we avoid the overhead of adding them up */
void
do_scrolling (WINDOW * main_win, struct struct_display **s_display,
              struct logger *logger, struct scrolling *scrolling, int cmd)
{
    int cur_y, cur_x, y, x, max = 0;
    getyx (main_win, cur_y, cur_x);     /* cursor */
    getmaxyx (main_win, y, x);

    int i = real_size_y + scrolling->init_scrl_main_win;
    int j = scrolling->init_scrl_main_win - 1;

    switch (cmd) {
     /* scroll down main window */
     case 1:
         if (!(i < logger->alloc_counter))
             return;
         scrollok (main_win, TRUE);
         wscrl (main_win, 1);
         scrollok (main_win, FALSE);

         if (s_display[i]->hits != 0)
             mvwprintw (main_win, cur_y, 2, "%d", s_display[i]->hits);
         /* draw headers */
         if ((i % 10) == 0)
             draw_header (main_win, s_display[i]->data, 0, cur_y, x, 1);
         else if ((i % 10) == 1) {
             draw_header (main_win, s_display[i]->data, 0, cur_y, x, 2);
         } else if (((s_display[i]->module == UNIQUE_VISITORS))
                    && ((i % 10 >= 3) && (i % 10 <= 8)
                        && (s_display[i]->hits != 0))) {
             max = get_max_value (s_display, logger, UNIQUE_VISITORS);
             create_graphs (main_win, s_display, logger, i, 1, max);

         } else if (((s_display[i]->module == OS))
                    && ((i % 10 >= 3) && (i % 10 <= 8)
                        && (s_display[i]->hits != 0))) {
             max = get_max_value (s_display, logger, OS);
             create_graphs (main_win, s_display, logger, i, 1, max);

         } else if (((s_display[i]->module == BROWSERS))
                    && ((i % 10 >= 3) && (i % 10 <= 8)
                        && (s_display[i]->hits != 0))) {
             max = get_max_value (s_display, logger, BROWSERS);
             create_graphs (main_win, s_display, logger, i, 1, max);

         } else if (((s_display[i]->module == HOSTS))
                    && ((i % 10 >= 3) && (i % 10 <= 8)
                        && (s_display[i]->hits != 0))) {
             max = get_max_value (s_display, logger, HOSTS);
             create_graphs (main_win, s_display, logger, i, 8, max);

         } else if (((s_display[i]->module == STATUS_CODES))
                    && ((i % 10 >= 3) && (i % 10 <= 8)
                        && (s_display[i]->hits != 0))) {
             max = get_max_value (s_display, logger, STATUS_CODES);
             create_graphs (main_win, s_display, logger, i, 9, max);

         } else
             mvwprintw (main_win, cur_y, 10, "%s", s_display[i]->data);

         scrolling->scrl_main_win++;
         scrolling->init_scrl_main_win++;
         break;
     /* scroll up main window */
     case 0:
         if (!(j >= 0))
             return;
         scrollok (main_win, TRUE);
         wscrl (main_win, -1);
         scrollok (main_win, FALSE);

         if (s_display[j]->hits != 0)
             mvwprintw (main_win, 0, 2, "%d", s_display[j]->hits);
         /* draw headers */
         if ((j % 10) == 0)
             draw_header (main_win, s_display[j]->data, 0, 0, x, 1);
         else if ((j % 10) == 1) {
             draw_header (main_win, s_display[j]->data, 0, 0, x, 2);
         } else if (((s_display[j]->module == UNIQUE_VISITORS))
                    && ((j % 10 >= 3) && (j % 10 <= 8)
                        && (s_display[j]->hits != 0))) {
             max = get_max_value (s_display, logger, UNIQUE_VISITORS);
             create_graphs (main_win, s_display, logger, j, 1, max);
         } else if (((s_display[j]->module == OS))
                    && ((j % 10 >= 3) && (j % 10 <= 8)
                        && (s_display[j]->hits != 0))) {
             max = get_max_value (s_display, logger, OS);
             create_graphs (main_win, s_display, logger, j, 1, max);
         } else if (((s_display[j]->module == BROWSERS))
                    && ((j % 10 >= 3) && (j % 10 <= 8)
                        && (s_display[j]->hits != 0))) {
             max = get_max_value (s_display, logger, BROWSERS);
             create_graphs (main_win, s_display, logger, j, 1, max);
         } else if (((s_display[j]->module == HOSTS))
                    && ((j % 10 >= 3) && (j % 10 <= 8)
                        && (s_display[j]->hits != 0))) {
             max = get_max_value (s_display, logger, HOSTS);
             create_graphs (main_win, s_display, logger, j, 8, max);

         } else if (((s_display[j]->module == STATUS_CODES))
                    && ((j % 10 >= 3) && (j % 10 <= 8)
                        && (s_display[j]->hits != 0))) {
             max = get_max_value (s_display, logger, STATUS_CODES);
             create_graphs (main_win, s_display, logger, j, 9, max);

         } else
             mvwprintw (main_win, cur_y, 10, "%s", s_display[j]->data);

         scrolling->scrl_main_win--;
         scrolling->init_scrl_main_win--;
         break;
    }
}

static void
load_help_popup_content (WINDOW * inner_win, int where,
                         struct scrolling *scrolling)
{
    int y, x;
    getmaxyx (inner_win, y, x);

    switch (where) {
     /* scroll down */
     case 1:
         if (((size_t) (scrolling->scrl_help_win - 5)) >= help_main_size ())
             return;
         scrollok (inner_win, TRUE);
         wscrl (inner_win, 1);
         scrollok (inner_win, FALSE);
         wmove (inner_win, y - 1, 2);
         /* minus help_win offset - 5 */
         mvwaddstr (inner_win, y - 1, 2,
                    help_main[scrolling->scrl_help_win - 5]);
         scrolling->scrl_help_win++;
         break;
     /* scroll up */
     case 0:
         if ((scrolling->scrl_help_win - y) - 5 <= 0)
             return;
         scrollok (inner_win, TRUE);
         wscrl (inner_win, -1);
         scrollok (inner_win, FALSE);
         wmove (inner_win, 0, 2);
         /* minus help_win offset - 6 */
         mvwaddstr (inner_win, 0, 2,
                    help_main[(scrolling->scrl_help_win - y) - 6]);
         scrolling->scrl_help_win--;
         break;
    }
    wrefresh (inner_win);
}

void
load_help_popup (WINDOW * help_win)
{
    WINDOW *inner_win;
    int y, x, c;
    size_t sz;
    struct scrolling scrolling;

    getmaxyx (help_win, y, x);
    draw_header (help_win,
                 "  Use cursor UP/DOWN - PGUP/PGDOWN to scroll. q:quit", 0, 1,
                 x, 2);
    wborder (help_win, '|', '|', '-', '-', '+', '+', '+', '+');
    inner_win = newwin (y - 5, x - 4, 11, 21);
    sz = help_main_size ();

    int i, m = 0;
    for (i = 0; (i < y) && (((size_t) i) < sz); i++, m++)
        mvwaddstr (inner_win, m, 2, help_main[i]);

    scrolling.scrl_help_win = y;
    wmove (help_win, y, 0);
    wrefresh (help_win);
    wrefresh (inner_win);

    while ((c = wgetch (stdscr)) != 'q') {
        switch (c) {
         case KEY_DOWN:
             (void) load_help_popup_content (inner_win, 1, &scrolling);
             break;
         case KEY_UP:
             (void) load_help_popup_content (inner_win, 0, &scrolling);
             break;
        }
        wrefresh (help_win);
    }
}

void
load_reverse_dns_popup (WINDOW * ip_detail_win, char *addr)
{
    int y, x, c, quit = 1;
    char *my_addr = reverse_ip (addr);
    const char *location;

    getmaxyx (ip_detail_win, y, x);
    draw_header (ip_detail_win, "  Reverse DNS lookup - q:quit", 0, 1, x - 1,
                 2);
    wborder (ip_detail_win, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw (ip_detail_win, 3, 2, "Reverse DNS for address: %s", addr);
    mvwprintw (ip_detail_win, 4, 2, "%s", my_addr);

    /* geolocation data */
    GeoIP *gi;
    gi = GeoIP_new (GEOIP_STANDARD);
    location = GeoIP_country_name_by_name (gi, addr);
    GeoIP_delete (gi);
    if (location == NULL)
        location = "Not found";
    mvwprintw (ip_detail_win, 5, 2, "Country: %s", location);
    free (my_addr);

    wrefresh (ip_detail_win);
    /* ###TODO: resize child windows. */
    /* for now we can close them up */
    while (quit) {
        c = wgetch (stdscr);
        switch (c) {
         case KEY_RESIZE:
         case 'q':
             quit = 0;
             break;
        }
    }
    render_screens ();
    return;
}

static ITEM **
get_menu_items (struct struct_holder **s_holder, struct logger *logger,
                int choices, int sort)
{
    int i;
    char *hits = NULL;
    char buf[12] = "";
    char *buffer_date = NULL;   /* max length of date */
    struct tm tm;
    ITEM **items;

    memset (&tm, 0, sizeof (tm));

    /* sort the struct prior to display */
    if (sort)
        qsort (s_holder, logger->counter, sizeof (struct struct_holder *),
               struct_cmp_by_hits);
    else
        qsort (s_holder, logger->counter, sizeof (struct struct_holder *),
               struct_cmp);

    items = (ITEM **) malloc (sizeof (ITEM *) * (choices + 1));

    for (i = 0; i < choices; ++i) {
        hits = (char *) malloc (sizeof (char) * 8);
        if (hits == NULL)
            error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                           "Unable to allocate memory");
        sprintf (hits, "%3i", s_holder[i]->hits);
        if (logger->current_module == 1) {
            buffer_date = (char *) malloc (sizeof (char) * 13);
            if (buffer_date == NULL)
                error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                               "Unable to allocate memory");
            strptime (s_holder[i]->data, "%Y%m%d", &tm);
            strftime (buf, sizeof (buf), "%d/%b/%Y", &tm);
            sprintf (buffer_date, "%s", buf);
            items[i] = new_item (hits, buffer_date);
        } else
            items[i] = new_item (hits, s_holder[i]->data);
    }

    items[i] = (ITEM *) NULL;

    return items;
}

static MENU *
set_menu (WINDOW * my_menu_win, ITEM ** items, struct logger *logger)
{
    MENU *my_menu = NULL;
    int x = 0;
    int y = 0;

    getmaxyx (my_menu_win, y, x);
    my_menu = new_menu (items);

    keypad (my_menu_win, TRUE);

    /* set main window and sub window */
    set_menu_win (my_menu, my_menu_win);
    set_menu_sub (my_menu, derwin (my_menu_win, y - 6, x - 2, 4, 1));
    set_menu_format (my_menu, y - 6, 1);

    /* set menu mark */
    set_menu_mark (my_menu, " => ");

    draw_header (my_menu_win,
                 "  Use cursor UP/DOWN - PGUP/PGDOWN to scroll. q:quit", 0, 1,
                 x, 2);
    draw_header (my_menu_win, module_names[logger->current_module - 1], 0, 2,
                 x, 1);
    wborder (my_menu_win, '|', '|', '-', '-', '+', '+', '+', '+');
    return my_menu;
}

static void
load_popup_content (WINDOW * my_menu_win, int choices,
                    struct struct_holder **s_holder, struct logger *logger,
                    int sort)
{
    wclrtoeol (my_menu_win);
    items = get_menu_items (s_holder, logger, choices, sort);
    my_menu = set_menu (my_menu_win, items, logger);
    post_menu (my_menu);
    wrefresh (my_menu_win);
}

static void
load_popup_free_items (ITEM ** items, struct logger *logger)
{
    int i;
    char *name = NULL;
    char *description = NULL;

    /* clean up stuff */
    i = 0;
    while ((ITEM *) NULL != items[i]) {
        name = (char *) item_name (items[i]);
        free (name);
        if (logger->current_module == 1) {
            description = (char *) item_description (items[i]);
            free (description);
        }
        free_item (items[i]);
        i++;
    }
    free (items);
    items = NULL;
}

static ITEM *
search_request (MENU * my_menu, const char *input)
{
    char *str = NULL;
    str = strdup (input);

    ITEM *item_ptr = NULL;

    if (input != NULL) {
        int i = -1, j = -1, response = 0;
        j = item_index (current_item (my_menu));

        for (i = j + 1; i < item_count (my_menu) && !response; i++) {
            if (strstr (item_description (menu_items (my_menu)[i]), input))
                response = 1;
        }
        if (response)
            item_ptr = menu_items (my_menu)[i - 1];
    }
    free (str);
    return item_ptr;
}

void
load_popup (WINDOW * my_menu_win, struct struct_holder **s_holder,
            struct logger *logger)
{
    WINDOW *ip_detail_win;
    ITEM *query = NULL;

    /*###TODO: perhaps let the user change the size of MAX_CHOICES */ 
    int choices = MAX_CHOICES, c, x, y;
    char input[BUFFER] = "";

    GHashTable *hash_table = NULL;

    switch (logger->current_module) {
     case UNIQUE_VISITORS:
         hash_table = ht_unique_vis;
         break;
     case REQUESTS:
         hash_table = ht_requests;
         break;
     case REQUESTS_STATIC:
         hash_table = ht_requests_static;
         break;
     case REFERRERS:
         hash_table = ht_referers;
         break;
     case NOT_FOUND:
         hash_table = ht_not_found_requests;
         break;
     case OS:
         hash_table = ht_os;
         break;
     case BROWSERS:
         hash_table = ht_browsers;
         break;
     case HOSTS:
         hash_table = ht_hosts;
         break;
     case STATUS_CODES:
         hash_table = ht_status_code;
         break;
     case REFERRING_SITES:
         hash_table = ht_referring_sites;
         break;
     case KEYPHRASES:
         hash_table = ht_keyphrases;
         break;
    }
    getmaxyx (my_menu_win, y, x);
    MALLOC_STRUCT (s_holder, g_hash_table_size (hash_table));

    int i = 0, quit = 1;
    GHashTableIter iter;
    gpointer k = NULL;
    gpointer v = NULL;

    g_hash_table_iter_init (&iter, hash_table);
    while (g_hash_table_iter_next (&iter, &k, &v)) {
        s_holder[i]->data = (gchar *) k;
        s_holder[i++]->hits = GPOINTER_TO_INT (v);
        logger->counter++;
    }

    /* again, letting the user to set the max number 
     * might be a better way to go */
    choices =
        (g_hash_table_size (hash_table) >
         100) ? MAX_CHOICES : g_hash_table_size (hash_table);
    load_popup_content (my_menu_win, choices, s_holder, logger, 1);

    while (quit) {
        c = wgetch (stdscr);
        switch (c) {
         case KEY_DOWN:
             menu_driver (my_menu, REQ_DOWN_ITEM);
             break;
         case KEY_UP:
             menu_driver (my_menu, REQ_UP_ITEM);
             break;
         case KEY_NPAGE:
             menu_driver (my_menu, REQ_SCR_DPAGE);
             break;
         case KEY_PPAGE:
             menu_driver (my_menu, REQ_SCR_UPAGE);
             break;
         case '/':
             /* set the whole ui for search */
             wattron (my_menu_win, COLOR_PAIR (COL_CYAN));
             mvwhline (my_menu_win, y - 2, 2, ' ', x - 4);
             mvwaddnstr (my_menu_win, y - 2, 2, "/", 20);
             wattroff (my_menu_win, COLOR_PAIR (COL_CYAN));
             nocbreak ();
             echo ();
             curs_set (1);
             wattron (my_menu_win, COLOR_PAIR (COL_CYAN));
             wscanw (my_menu_win, "%s", input);
             wattroff (my_menu_win, COLOR_PAIR (COL_CYAN));
             cbreak ();
             noecho ();
             halfdelay (10);
             nonl ();
             intrflush (stdscr, FALSE);
             curs_set (0);

             query = search_request (my_menu, input);
             if (query != NULL) {
                 while (FALSE == item_visible (query))
                     menu_driver (my_menu, REQ_SCR_DPAGE);
                 set_current_item (my_menu, query);
             } else {
                 wattron (my_menu_win, COLOR_PAIR (WHITE_RED));
                 mvwhline (my_menu_win, y - 2, 2, ' ', x - 4);
                 mvwaddnstr (my_menu_win, y - 2, 2, "Pattern not found", 20);
                 wattroff (my_menu_win, COLOR_PAIR (WHITE_RED));
             }
             break;
         case 'n':
             if (strlen (input) == 0)
                 break;
             query = search_request (my_menu, input);
             if (query != NULL) {
                 while (FALSE == item_visible (query))
                     menu_driver (my_menu, REQ_SCR_DPAGE);
                 set_current_item (my_menu, query);
             } else {
                 wattron (my_menu_win, COLOR_PAIR (WHITE_RED));
                 mvwhline (my_menu_win, y - 2, 2, ' ', x - 4);
                 mvwaddnstr (my_menu_win, y - 2, 2, "search hit BOTTOM", 20);
                 wattroff (my_menu_win, COLOR_PAIR (WHITE_RED));
             }
             break;
         case 116:
             menu_driver (my_menu, REQ_FIRST_ITEM);
             break;
         case 98:
             menu_driver (my_menu, REQ_LAST_ITEM);
             break;
         case 's':
             if (logger->current_module != 1)
                 break;
             unpost_menu (my_menu);
             free_menu (my_menu);
             my_menu = NULL;
             load_popup_free_items (items, logger);

             load_popup_content (my_menu_win, choices, s_holder, logger, 0);
             break;
         case 'S':
             if (logger->current_module != 1)
                 break;
             unpost_menu (my_menu);
             free_menu (my_menu);
             my_menu = NULL;
             load_popup_free_items (items, logger);
             load_popup_content (my_menu_win, choices, s_holder, logger, 1);
             break;
         case 10:
         case KEY_RIGHT:
             if (logger->current_module != 8)
                 break;

             ITEM *cur;
             cur = current_item (my_menu);
             /* no current item? break */
             if (cur == NULL)
                 break;

             ip_detail_win = newwin (y - 13, x - 5, 10, 21);
             char addrs[32];
             sprintf (addrs, "%s", item_description (cur));
             load_reverse_dns_popup (ip_detail_win, addrs);
             pos_menu_cursor (my_menu);
             wrefresh (ip_detail_win);
             touchwin (my_menu_win);
             close_win (ip_detail_win);
             break;
             /* ###TODO: resize child windows. */
             /* for now we can close them up */
         case KEY_RESIZE:
         case 'q':
             quit = 0;
             break;
        }
        wrefresh (my_menu_win);
    }

    int f;
    for (f = 0; f < logger->counter; f++)
        free (s_holder[f]);
    free (s_holder);
    logger->counter = 0;

    /* unpost and free all the memory taken up */
    unpost_menu (my_menu);
    free_menu (my_menu);

    /* clean up stuff */
    my_menu = NULL;
    load_popup_free_items (items, logger);
    render_screens ();
}
