/* 
 * File:   main.c
 * Author: Samy Vilar
 *
 * Created on November 24, 2011, 12:55 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>

void error(char msg[])
{
    printf("Error: %s", msg);
    exit(-1);
}

typedef struct Point // Represents a point at (x, y)
{
    int x, y;
} Point;

typedef struct Point_LL // Represents a link list of points 
{
    Point point;
    struct Point_LL *next;
} Point_LL;

typedef struct Interval // Represents an interval 
{
    int min, max;
} Interval;

Point_LL *get_new_point_ll(int x, int y) // create a new link for a point link list
{
    Point_LL *point = (Point_LL *)malloc(sizeof(Point_LL));
    point->point.x  = x;
    point->point.y  = y;
    point->next     = NULL;
    
    return point;
}

typedef struct Path // Represents a path as a link list of points of the yellow, red, and blue server, keeps the total distance ...
{
    Point_LL *yellow_ll, *blue_ll, *red_ll;            
    double distance;
} Path;

typedef struct System // General structure for moving data around subprocesses ...
{   
    Interval xintv, yintv;
    Point_LL *points_ll, yellow_server, red_server, blue_server;
    Point_LL **points_ra;
    
    Path opt_path, inopt_path;     
    unsigned int points_count;
    
    double inopt_total_distance, opt_total_distance;                    
} System;

typedef struct Configuration // Configuration used to keep track of the previous state of the system, and distance so far ...
{
    int prev_yellow, prev_red, prev_blue;    
    double distance;
} Configuration;

double get_distance(Point_LL *point_1, Point_LL *point_2) // Get distance between 2 points ...
{   return sqrt( (pow(((double)((point_2->point.x - point_1->point.x))), 2.0) + 
                  pow(((double)(point_2->point.y - point_1->point.y)), 2.0)) );        
}

Configuration *get_configuration(int yellow, int red, int blue, double distance) // Get a new configuration ...
{
    Configuration *conf = (Configuration *)malloc(sizeof(Configuration));
    
    conf->prev_yellow   = yellow;
    conf->prev_red      = red;
    conf->prev_blue     = blue;
    conf->distance      = distance;
    
    return conf;            
}

double find_opt_rec(unsigned int curr_yellow, 
                    unsigned int curr_red, 
                    unsigned int curr_blue, 
                    unsigned int request, 
                    Point_LL **requests,
                    unsigned int request_count
                    ) // Find the optimal distance, recursively, used only for testing and verification, warning 3^n run-time.
{
    if (request >= request_count)
        return 0;
    
    double distance_yellow, distance_red, distance_blue;                
            
    distance_yellow = get_distance(requests[request], requests[curr_yellow]) + find_opt_rec(request,     curr_red, curr_blue, (request + 1), requests, request_count);
    distance_red    = get_distance(requests[request], requests[curr_red])    + find_opt_rec(curr_yellow, request,  curr_blue, (request + 1), requests, request_count);
    distance_blue   = get_distance(requests[request], requests[curr_blue])   + find_opt_rec(curr_yellow, curr_red, request,   (request + 1), requests, request_count);        
    
    if      (distance_yellow < distance_red    && distance_yellow < distance_blue)                                              
        return distance_yellow;            
    else if (distance_red   < distance_yellow  && distance_red < distance_blue)                    
        return distance_red;            
    else if (distance_blue  < distance_yellow  && distance_blue < distance_red)                                    
        return distance_blue;           
    else                            
        return distance_red;            
}

// check which server was last moved!
int get_server(int curr_yellow, int curr_red, int curr_blue, int prev_yellow, int prev_red, int prev_blue)
{
    if      (curr_yellow != prev_yellow && curr_blue   == prev_blue   && curr_red  == prev_red)    
        return 0;    
    else if (curr_red    != prev_red    && curr_yellow == prev_yellow && curr_blue == prev_blue)
        return 1;
    else if (curr_blue   != prev_blue  && curr_yellow  == prev_yellow && curr_red  == prev_red)
        return 2;
    else
        error("Only one server can move at any given moment!");
    return -1;
}

// Find optimal solution using dynamic programming ...
void set_opt_solution(System *system)
{   
    unsigned int index = 0, blue_index = 0, red_index = 0, yellow_index = 0, points_count = system->points_count + 3;   
                      
    // Table of configurations ... initially all NULL
    Configuration *****table = (Configuration *****)malloc(sizeof(Configuration ****) * points_count);
    for (yellow_index = 0; yellow_index < points_count; yellow_index++)
    {
        table[yellow_index] = (Configuration ****)malloc(sizeof(Configuration ***) * points_count);        
        for (red_index = 0; red_index < points_count; red_index++)
        {
            table[yellow_index][red_index] = (Configuration ***)malloc(sizeof(Configuration **) * points_count);
            for (blue_index = 0; blue_index < points_count; blue_index++)
            {
                table[yellow_index][red_index][blue_index] = (Configuration **)malloc(sizeof(Configuration *) * points_count);
                for (index = 0; index < points_count; index++)
                    table[yellow_index][red_index][blue_index][index] = NULL;
            }
        }        
    }                        
    // Initial configurations ...
    table[0][1][3][3] = get_configuration(0, 1, 2, get_distance(system->points_ra[3], system->points_ra[2]));                            
    table[0][3][2][3] = get_configuration(0, 1, 2, get_distance(system->points_ra[3], system->points_ra[1]));    
    table[3][1][2][3] = get_configuration(0, 1, 2, get_distance(system->points_ra[3], system->points_ra[0])); 
                    
    Configuration *min = NULL; unsigned int prev_yellow, prev_red, prev_blue; double prev_dist;
        
    // go through each configuration either create or update using the least distance, and keep track of the minimun configuration that serverd the last request
    unsigned int yellow = 0, red = 0, blue = 0; Configuration *temp = NULL;
    for (yellow_index = 0; yellow_index < points_count; yellow_index++)    
        for (red_index = 1; red_index < points_count; red_index++)
            for (blue_index = 2; blue_index < points_count; blue_index++)
                for (index = 3; index < points_count - 1; index++)
                    if (table[yellow_index][red_index][blue_index][index])
                    {
                        prev_yellow = table[yellow_index][red_index][blue_index][index]->prev_yellow;
                        prev_red    = table[yellow_index][red_index][blue_index][index]->prev_red;      
                        prev_blue   = table[yellow_index][red_index][blue_index][index]->prev_blue;
                        prev_dist   = table[yellow_index][red_index][blue_index][index]->distance;
                        
                        if ((blue_index == index || red_index == index || yellow_index == index))
                        {                                                        
                            
                            if (table[yellow_index][red_index][index + 1][index + 1])
                            {                                
                                temp = get_configuration(yellow_index, red_index, blue_index, (prev_dist + get_distance(system->points_ra[blue_index], system->points_ra[index + 1])));
                                if (table[yellow_index][red_index][index + 1][index + 1]->distance > temp->distance)
                                {    table[yellow_index][red_index][index + 1][index + 1] = temp;       }                                    
                            }
                            else
                                table[yellow_index][red_index][index + 1][index + 1] = get_configuration(yellow_index, red_index, blue_index, 
                                    (prev_dist + get_distance(system->points_ra[blue_index], system->points_ra[index + 1])));
                                                        
                            if (table[yellow_index][index + 1][blue_index][index + 1])
                            {                             
                                temp = get_configuration(yellow_index, red_index, blue_index, 
                                    (prev_dist + get_distance(system->points_ra[red_index], system->points_ra[index + 1])));                             
                                if (table[yellow_index][index + 1][blue_index][index + 1]->distance > temp->distance)
                                {    table[yellow_index][index + 1][blue_index][index + 1] = temp;      }
                            }
                            else
                                table[yellow_index][index + 1][blue_index][index + 1] = get_configuration(yellow_index, red_index, blue_index, 
                                    (prev_dist + get_distance(system->points_ra[red_index], system->points_ra[index + 1])));
                            
                            if (table[index + 1][red_index][blue_index][index + 1])
                            {                                
                                temp = get_configuration(yellow_index, red_index, blue_index, 
                                    (prev_dist + get_distance(system->points_ra[yellow_index], system->points_ra[index + 1])));
                                if (table[index + 1][red_index][blue_index][index + 1]->distance > temp->distance)
                                {    table[index + 1][red_index][blue_index][index + 1] = temp;         }
                            }
                            else                                
                                table[index + 1][red_index][blue_index][index + 1] = get_configuration(yellow_index, red_index, blue_index, 
                                    (prev_dist + get_distance(system->points_ra[yellow_index], system->points_ra[index + 1])));                                                                                                                                            
                            
                            if ((index + 2) == points_count)
                            {
                                if (!min) 
                                {
                                    min = table[yellow_index][red_index][index + 1][index + 1];
                                    yellow = yellow_index;
                                    red = red_index;
                                    blue = index + 1;
                                }
                                
                                if (min->distance > table[yellow_index][red_index][index + 1][index + 1]->distance)
                                {                                    
                                    min = table[yellow_index][red_index][index + 1][index + 1];
                                    yellow = yellow_index;
                                    red = red_index;
                                    blue = index + 1;
                                                                        
                                }
                                
                                if (min->distance > table[yellow_index][index + 1][blue_index][index + 1]->distance)
                                {                                 
                                    min = table[yellow_index][index + 1][blue_index][index + 1];
                                    yellow = yellow_index;
                                    red = index + 1;
                                    blue = blue_index;
                                }
                                
                                if (min->distance > table[index + 1][red_index][blue_index][index + 1]->distance)
                                {                                 
                                    min = table[index + 1][red_index][blue_index][index + 1];
                                    yellow = index + 1;
                                    red = red_index;
                                    blue = blue_index;
                                }                                                                
                            }                                                                                                                
                        }                                                                                                                        
                    }        

    if (!min)   // One request ...
    {
        min = table[0][1][3][3];
        yellow = 0;
        red = 1;
        blue = 3;
        
        if (min->distance > table[0][1][3][3]->distance)
        {
            min = table[0][1][3][3];
            yellow = 0;
            red = 1;
            blue = 3;        
        }
        else if (min->distance > table[0][3][2][3]->distance)
        {
            min = table[0][3][2][3];
            yellow = 0;
            red = 3;
            blue = 2;
        }
        else if (min->distance > table[3][1][2][3]->distance)
        {
            min = table[3][1][2][3];
            yellow = 3;
            red = 1;
            blue = 2;
        }
    }
    
    system->opt_total_distance = min->distance; 
                    
    Point_LL *temp_ll = NULL, *prev_blue_ll = NULL, *prev_red_ll = NULL, *prev_yellow_ll = NULL; 
    int server;
                
    while (index > 2) // build optimal path by building the link-list backwards ...
    {                        
        server = get_server(yellow, red, blue, min->prev_yellow, min->prev_red, min->prev_blue);                
        
        if (server == 0)
        {
            if (!prev_yellow_ll)            
            {    prev_yellow_ll = get_new_point_ll(system->points_ra[index]->point.x, system->points_ra[index]->point.y);}                                                    
            else
            {
                temp_ll = get_new_point_ll(system->points_ra[index]->point.x, system->points_ra[index]->point.y);
                temp_ll->next = prev_yellow_ll;
                prev_yellow_ll = temp_ll;                
            }
        }
        else if (server == 1)
        {
            if (!prev_red_ll)            
            {   prev_red_ll = get_new_point_ll(system->points_ra[index]->point.x, system->points_ra[index]->point.y);  }          
            else
            {
                temp_ll  = get_new_point_ll(system->points_ra[index]->point.x, system->points_ra[index]->point.y);
                temp_ll->next = prev_red_ll;
                prev_red_ll = temp_ll;
            }
        }
        else if (server == 2)
        {
            if (!prev_blue_ll)            
            {   prev_blue_ll = get_new_point_ll(system->points_ra[index]->point.x, system->points_ra[index]->point.y);   }
            else
            {
                temp_ll = get_new_point_ll(system->points_ra[index]->point.x, system->points_ra[index]->point.y);                
                temp_ll->next = prev_blue_ll;
                prev_blue_ll = temp_ll;
                
            }
        }
        
        yellow = min->prev_yellow; red = min->prev_red; blue = min->prev_blue;
        min = table[yellow][red][blue][--index];                    
    }
    
    temp_ll = get_new_point_ll(system->points_ra[0]->point.x, system->points_ra[0]->point.y);
    temp_ll->next = prev_yellow_ll;
    system->opt_path.yellow_ll = temp_ll;
    
    temp_ll = get_new_point_ll(system->points_ra[1]->point.x, system->points_ra[1]->point.y);
    temp_ll->next = prev_red_ll;
    system->opt_path.red_ll = temp_ll;
    
    temp_ll = get_new_point_ll(system->points_ra[2]->point.x, system->points_ra[2]->point.y);
    temp_ll->next = prev_blue_ll;
    system->opt_path.blue_ll = temp_ll;


    for (yellow_index = 0; yellow_index < points_count; yellow_index++)
    {
        for (red_index = 0; red_index < points_count; red_index++)
        {
            for (blue_index = 0; blue_index < points_count; blue_index++)
            {
                for (index = 0; index < points_count; index++)
                    if (table[yellow_index][red_index][blue_index][index])
                        free(table[yellow_index][red_index][blue_index][index]);
                free(table[yellow_index][red_index][blue_index]);
            }
            free(table[yellow_index][red_index]);
        }
        free(table[yellow_index]);
    }
    free(table);
}

void set_inopt_solution(System *system) // Set non optimal path, by always selecting the closest server
{
    Point_LL *curr_blue   = system->inopt_path.blue_ll   = get_new_point_ll(system->blue_server.point.x,   system->blue_server.point.y);        
    Point_LL *curr_red    = system->inopt_path.red_ll    = get_new_point_ll(system->red_server.point.x,    system->red_server.point.y);        
    Point_LL *curr_yellow = system->inopt_path.yellow_ll = get_new_point_ll(system->yellow_server.point.x, system->yellow_server.point.y);    
    
    Point_LL *curr_point = system->points_ll;
    double distance_1, distance_2, distance_3;
    while (curr_point)
    {
        distance_1 = get_distance(curr_blue,   curr_point);
        distance_2 = get_distance(curr_red,    curr_point);
        distance_3 = get_distance(curr_yellow, curr_point);                
        
        if (distance_1 < distance_2 && distance_1 < distance_3)
        {
            curr_blue->next = get_new_point_ll(curr_point->point.x, curr_point->point.y);            
            curr_blue = curr_blue->next;                        
            system->inopt_total_distance += distance_1;
        }
        else if (distance_2 < distance_1 && distance_2 < distance_3)
        {
            curr_red->next = get_new_point_ll(curr_point->point.x, curr_point->point.y);            
            curr_red = curr_red->next;            
            system->inopt_total_distance += distance_2;
        }
        else if (distance_3 < distance_1 && distance_3 < distance_2)
        {
            curr_yellow->next = get_new_point_ll(curr_point->point.x, curr_point->point.y);            
            curr_yellow = curr_yellow->next;           
            system->inopt_total_distance += distance_3;
        }
        else
        {
            curr_red->next = get_new_point_ll(curr_point->point.x, curr_point->point.y);            
            curr_red = curr_red->next;            
            system->inopt_total_distance += distance_1;
        }
        curr_point = curr_point->next;
    }                   
}

void set_solutions(System *system) // set both solutions as well as do any other work ...
{
    Point_LL *curr = system->points_ll;
    system->points_ra = (Point_LL **)malloc(sizeof(Point_LL *) * (system->points_count + 3));
    system->inopt_total_distance = 0;
    system->opt_total_distance   = 0;
    
    system->points_ra[0] = &system->yellow_server;
    system->points_ra[1] = &system->red_server;
    system->points_ra[2] = &system->blue_server;
        
    unsigned int index = 3;        
    while (curr)
    {
        system->points_ra[index++]  = curr;
        curr = curr->next;
    }
    
    curr = system->points_ll;
    
    set_inopt_solution(system);
        
    // used to verify the dynamic programming is find the correct distance.
    // double distance = find_opt_rec(0, 1, 2, 3, system->points_ra, (system->points_count + 3));
    // printf("min distance recursive %f \n", distance);
    set_opt_solution(system);
}

void free_system(System *system)
{
    if (!system)
        return ;

    Point_LL *temp = NULL;
    while (system->points_ll)
    {
        temp = system->points_ll->next;
        free(system->points_ll);
        system->points_ll = temp;
    }

    free(system->points_ra);
    free(system);
}

void set_points(System *system, int argc, char **argv) // ask for points, display them as well as both paths.
{
    Display             *display_ptr;
    Screen              *screen_ptr;
    char                *display_name = NULL;
    int                 screen_num;
    Colormap            color_map;
    int                 win_x, win_y, border_width;        
    unsigned int        display_width, display_height, win_width, win_height;    
    
    XSizeHints          *size_hints;
    XWMHints            *wm_hints;
    XClassHint          *class_hints;    
    XEvent              report;    
    Window              win;        
    
    char *win_name_string = malloc(100);        
    
    char *icon_name_string = "Icon for Window";    

    XTextProperty win_name, icon_name;
        
    GC gc, gc_blue_thin, gc_blue_thick, gc_red_thin, gc_red_thick, gc_yellow_thin, gc_yellow_thick, 
            gc_white_thin, gc_white_thick, gc_black;
    unsigned long valuemask = 0;
    XGCValues gc_values, gc_blue_values, gc_red_values, gc_white_values, gc_black_values;
    
    XColor tmp_color1, tmp_color2;
    
    if ((display_ptr = XOpenDisplay(display_name)) == NULL)
        error("Could not open display. \n"); 

    screen_num          = DefaultScreen         (display_ptr);
    screen_ptr          = DefaultScreenOfDisplay(display_ptr);
    color_map           = XDefaultColormap      (display_ptr, screen_num);
    display_width       = DisplayWidth          (display_ptr, screen_num);
    display_height      = DisplayHeight         (display_ptr, screen_num);

    /* creating the window */
    
    
    border_width = 4;
    win_x        = (display_width/2)  - (abs(system->xintv.max - system->xintv.min)+ border_width*2)/2; 
    win_y        = (display_height/2) - (abs(system->yintv.max - system->yintv.min)+ border_width*2)/2;
        
    win_width    = abs(system->xintv.max - system->xintv.min) + border_width*2;
    win_height   = abs(system->yintv.max - system->yintv.min) + border_width*2; /*rectangular window*/
    

    win = XCreateSimpleWindow(display_ptr, RootWindow(display_ptr, screen_num),
                            win_x, win_y, win_width, win_height, border_width,
                            BlackPixel(display_ptr, screen_num),
                            WhitePixel(display_ptr, screen_num) );
  
    /* now try to put it on screen, this needs cooperation of window manager */
    size_hints          = XAllocSizeHints();
    wm_hints            = XAllocWMHints();
    class_hints         = XAllocClassHint();
    if( size_hints == NULL || wm_hints == NULL || class_hints == NULL )
        error("Error allocating memory for hints. \n");

    size_hints->flags = PPosition | PSize | PMinSize  ;
    size_hints->min_width = 60;
    size_hints->min_height = 60;

    XStringListToTextProperty(&win_name_string,  1, &win_name);
    XStringListToTextProperty(&icon_name_string, 1, &icon_name);

    wm_hints->flags             = StateHint | InputHint ;
    wm_hints->initial_state     = NormalState;
    wm_hints->input             = False;

    class_hints->res_name       = "x_use_example";
    class_hints->res_class      = "examples";

    XSetWMProperties(display_ptr, win, &win_name, &icon_name, argv, argc, size_hints, wm_hints, class_hints);

    /* what events do we want to receive */
    XSelectInput(display_ptr, win, ExposureMask | StructureNotifyMask | ButtonPressMask);

    /* finally: put window on screen */
    XMapWindow(display_ptr, win);
    XFlush(display_ptr);

    /* create graphics context, so that we may draw in this window */
    gc = XCreateGC(display_ptr, win, valuemask, &gc_values);
  
    XSetForeground    (display_ptr, gc, BlackPixel(display_ptr, screen_num));
    XSetLineAttributes(display_ptr, gc, 4, LineSolid, CapRound, JoinRound);
  
    gc_black = XCreateGC( display_ptr, win, valuemask, &gc_black_values);
    XSetLineAttributes( display_ptr, gc_black, 1, LineSolid, CapButt, JoinRound);
    if (XAllocNamedColor( display_ptr, color_map, "black", &tmp_color1, &tmp_color2) == 0)
        error("failed to get color black\n"); 
    else
        XSetForeground(display_ptr, gc_black, tmp_color1.pixel);    

    
    /* RED LINES ************************************************************/
    gc_red_thin = XCreateGC( display_ptr, win, valuemask, &gc_red_values);
    XSetLineAttributes( display_ptr, gc_red_thin, 1, LineSolid, CapButt, JoinRound);
    if (XAllocNamedColor( display_ptr, color_map, "red", &tmp_color1, &tmp_color2) == 0)
        error("failed to get color red\n"); 
    else
        XSetForeground(display_ptr, gc_red_thin, tmp_color1.pixel);    
    gc_red_thick = XCreateGC( display_ptr, win, valuemask, &gc_red_values);
    XSetLineAttributes( display_ptr, gc_red_thick, 4, LineSolid, CapButt, JoinRound);
    if (XAllocNamedColor( display_ptr, color_map, "red", &tmp_color1, &tmp_color2) == 0)
        error("failed to get color red\n"); 
    else
        XSetForeground(display_ptr, gc_red_thick, tmp_color1.pixel);
    /***********************************************************************/
      
    /* BLUE LINES ***********************************************************/
    gc_blue_thin = XCreateGC(display_ptr, win, valuemask, &gc_blue_values);
    XSetLineAttributes (display_ptr, gc_blue_thin, 1, LineSolid, CapRound, JoinRound);
    if (!XAllocNamedColor(display_ptr, color_map, "blue", &tmp_color1, &tmp_color2))
        error("failed to get color blue\n");     
    else
        XSetForeground(display_ptr, gc_blue_thin, tmp_color1.pixel);
    
    gc_blue_thick = XCreateGC(display_ptr, win, valuemask, &gc_blue_values);
    XSetLineAttributes (display_ptr, gc_blue_thick, 4, LineSolid, CapRound, JoinRound);
    if (!XAllocNamedColor(display_ptr, color_map, "blue", &tmp_color1, &tmp_color2))
        error("failed to get color blue\n");     
    else
        XSetForeground(display_ptr, gc_blue_thick, tmp_color1.pixel);
    /*************************************************************************/
    
    /* YELLOW LINES ***********************************************************/
    gc_yellow_thin = XCreateGC(display_ptr, win, valuemask, &gc_blue_values);
    XSetLineAttributes (display_ptr, gc_yellow_thin, 1, LineSolid, CapRound, JoinRound);
    if (!XAllocNamedColor(display_ptr, color_map, "yellow", &tmp_color1, &tmp_color2))
        error("failed to get color yellow\n");     
    else
        XSetForeground(display_ptr, gc_yellow_thin, tmp_color1.pixel);
    
    gc_yellow_thick = XCreateGC(display_ptr, win, valuemask, &gc_blue_values);
    XSetLineAttributes (display_ptr, gc_yellow_thick, 4, LineSolid, CapRound, JoinRound);
    if (!XAllocNamedColor(display_ptr, color_map, "yellow", &tmp_color1, &tmp_color2))
        error("failed to get color blue\n");     
    else
        XSetForeground(display_ptr, gc_yellow_thick, tmp_color1.pixel);
    /***************************************************************************/
    
    /* WHITE LINE ************************************************************/
    gc_white_thin = XCreateGC(display_ptr, win, valuemask, &gc_white_values);
    XSetLineAttributes (display_ptr, gc_white_thin, 1, LineSolid, CapButt , JoinBevel);
    if (!XAllocNamedColor(display_ptr, color_map, "white", &tmp_color1, &tmp_color2))
        error("failed to get color white\n");     
    else
        XSetForeground(display_ptr, gc_white_thin, tmp_color1.pixel);                    
    
    gc_white_thick = XCreateGC(display_ptr, win, valuemask, &gc_white_values);
    XSetLineAttributes (display_ptr, gc_white_thick, 4, LineSolid, CapButt , JoinBevel);
    if (!XAllocNamedColor(display_ptr, color_map, "white", &tmp_color1, &tmp_color2))
        error("failed to get color white\n");     
    else
        XSetForeground(display_ptr, gc_white_thick, tmp_color1.pixel);                    
    /****************************************************************************/
             
    int reading = 1;
    Point_LL *curr = NULL, *curr_blue = NULL, *curr_red = NULL, *curr_yellow = NULL, *p1 = NULL, *p2 = NULL;
    while (1)
    { 
        XNextEvent( display_ptr, &report );
        switch (report.type)
	    {
            case Expose:     
                XFillArc(display_ptr, win, gc_blue_thin,   system->blue_server.point.x,   system->blue_server.point.y,   8, 8, 0, 360*64);
                XFillArc(display_ptr, win, gc_red_thin,    system->red_server.point.x,    system->red_server.point.y,    8, 8, 0, 360*64);
                XFillArc(display_ptr, win, gc_yellow_thin, system->yellow_server.point.x, system->yellow_server.point.y, 8, 8, 0, 360*64);
                
                break;
                
            case ButtonPressMask:
                
                if (report.xbutton.button == Button1            && reading)
                {
                    if (!curr)
                        curr = system->points_ll = get_new_point_ll(report.xbutton.x, report.xbutton.y);
                    else
                    {
                        curr->next = get_new_point_ll(report.xbutton.x, report.xbutton.y);
                        curr = curr->next;
                    }      
                    system->points_count++;
                    XFillArc(display_ptr, win, gc_black,   curr->point.x,   curr->point.y,   8, 8, 0, 360*64);                    
                }                                
                else if (report.xbutton.button != Button1       && reading)
                {
                    reading = 0;
                    set_solutions(system);
                    
                    curr_blue   = system->inopt_path.blue_ll;
                    curr_red    = system->inopt_path.red_ll;
                    curr_yellow = system->inopt_path.yellow_ll;
                    
                    while (curr_blue->next)
                    {
                        p1 = curr_blue;                    
                        p2 = curr_blue->next;
                                        
                        XDrawLine(display_ptr, win, gc_blue_thin,
                                p1->point.x, p1->point.y, 
                                p2->point.x, p2->point.y);                    
                        curr_blue = curr_blue->next;
                    }
                    
                    while (curr_red->next)
                    {
                        p1 = curr_red;
                        p2 = curr_red->next;
                        
                        XDrawLine(display_ptr, win, gc_red_thin,
                                p1->point.x, p1->point.y, 
                                p2->point.x, p2->point.y);                    
                        curr_red = curr_red->next;                                                
                    }
                    
                    while (curr_yellow->next)
                    {
                        p1 = curr_yellow;
                        p2 = curr_yellow->next;                                                
                        
                        XDrawLine(display_ptr, win, gc_yellow_thin,
                                p1->point.x, p1->point.y, 
                                p2->point.x, p2->point.y);                    
                        curr_yellow = curr_yellow->next;                                                                        
                    }   
                    
                    curr_blue   = system->opt_path.blue_ll;
                    curr_red    = system->opt_path.red_ll;
                    curr_yellow = system->opt_path.yellow_ll;
                    
                    while (curr_blue->next)
                    {
                        p1 = curr_blue;                    
                        p2 = curr_blue->next;
                                        
                        XDrawLine(display_ptr, win, gc_blue_thick,
                                p1->point.x, p1->point.y, 
                                p2->point.x, p2->point.y);                    
                        curr_blue = curr_blue->next;
                    }
                    
                    while (curr_red->next)
                    {
                        p1 = curr_red;
                        p2 = curr_red->next;
                        
                        XDrawLine(display_ptr, win, gc_red_thick,
                                p1->point.x, p1->point.y, 
                                p2->point.x, p2->point.y);                    
                        curr_red = curr_red->next;                                                
                    }
                    
                    while (curr_yellow->next)
                    {
                        p1 = curr_yellow;
                        p2 = curr_yellow->next;
                        
                        XDrawLine(display_ptr, win, gc_yellow_thick,
                                p1->point.x, p1->point.y, 
                                p2->point.x, p2->point.y);                    
                        curr_yellow = curr_yellow->next;                                                                        
                    }
                }
                
                sprintf(win_name_string, "non-opt dist(%f) opt dist (%f) #points %i", system->inopt_total_distance, system->opt_total_distance, system->points_count);                                                                                
                XStoreName(display_ptr, win, win_name_string);                                
                
                break;
                
            case ConfigureNotify:          
              win_width = report.xconfigure.width;
              win_height = report.xconfigure.height;
              break;        
            default: break;
	    }
    }

    free_system(system);
}       


void init(int argc, char** argv)  // Initialize common system structure ...
{
    System *system = (System *)malloc(sizeof(System));
    system->xintv.min                   = 0;
    system->xintv.max                   = 600;
    system->yintv.min                   = 0;
    system->yintv.max                   = 400;
    
    system->yellow_server.point.x       = system->xintv.min;
    system->yellow_server.point.y       = system->yintv.min;
    
    system->red_server.point.x          = system->xintv.max;
    system->red_server.point.y          = system->yintv.min;
    
    system->blue_server.point.x         = (system->xintv.max - system->xintv.min) / 2;
    system->blue_server.point.y         = system->yintv.max;
    
    system->points_ll                   = NULL;
    system->points_ra                   = NULL;    
    system->points_count                = 0;
    
    set_points(system, argc, argv);    
}

int main(int argc, char** argv) 
{
    init(argc, argv);    // Initialize and begin system.

    return 0;
}
