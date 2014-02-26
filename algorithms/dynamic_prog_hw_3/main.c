/* 
 * File:   main.c
 * Author: Samy Vilar
 *
 * Created on November 17, 2011, 5:09 PM
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


typedef struct Point 
{
    int x, y;
    double distance;  /* (x, y) coordinate, distance so far from left most reachable point */      
    int int_segment_index, int_point_index;    /* index of vertex and point against line intersection, rand_access keys */
    struct Point_LL *prev; /* furthest left most direct point */
} Point;

typedef struct Point_LL /* Link List of points */
{
    Point point;
    struct Point_LL *next, *prev;
} Point_LL;

typedef struct Segment /* Segment or Line; start: starting point, end: ending point, points_ll: LL of all points (including start and end),
                        * curr_point used to conitnually add points from intersections, 
                        * points_rand_access: random_access, pointer table, useful for random access as well as sorting ... */
{    
    Point_LL *start, *end, *points_ll, *curr_point, **points_rand_access;   
    unsigned int point_count;    
} Segment;

typedef struct Segment_LL /* Link Lit of segments */
{
    Segment segment;
    struct Segment_LL *next;
} Segment_LL;

typedef struct Interval
{
    int min, max;    
} Interval;


typedef struct Data /* General Structure for moving data across all subroutines,
                     * segments_ll: LL of all segments, segments_rand_access: pointer table of all segments, both sorted
                     * all_points: All points including intersections sorting, use for sweep
                     * path: a LL of points generating a path ...
                     */ 
{
    Interval xintv, yintv;                        
    Segment_LL *segments_ll, **segments_rand_access;    
    Point_LL **all_points, *path;
    unsigned int  segment_count, points_count;        
} Data;

float get_eps() /* calculate machine epsilon, double is to accurate, float should suffice. */
{
    float eps = 1.0;    
    do { eps /= (float)2.0; } while (((float)1.0 + (eps/(float)2.0)) != (float)1.0);    
    return eps;
}

Point_LL *get_new_point_ll(int x, int y, Point_LL *prev, Point_LL *next)
{
    Point_LL *point                     = (Point_LL *)malloc(sizeof(Point_LL));
    point->point.x                      = x;
    point->point.y                      = y;        
    
    point->point.int_point_index        = -1;
    point->point.int_segment_index      = -1;
    point->point.distance               = -1;
    point->point.prev                   = NULL;
                
    point->prev                         = prev;
    point->next                         = next;    
    
    return point;
}

Point_LL *get_new_point_pair_ll(int x1, int y1, int x2, int y2)
{    
    Point_LL *point;
    if (x1 < x2)
    {
        point       = get_new_point_ll(x1, y1, NULL,  NULL);
        point->next = get_new_point_ll(x2, y2, point, NULL);        
    }
    else
    {
        point       = get_new_point_ll(x2, y2, NULL, NULL);
        point->next = get_new_point_ll(x1, y1, point, NULL);
    }        
    
    return point;
}

Segment_LL *get_new_segment_ll(int x1, int y1, int x2, int y2)
{
    Segment_LL *segment                 = (Segment_LL *)malloc(sizeof(Segment_LL));
            
    segment->segment.points_ll          = get_new_point_pair_ll(x1, y1, x2, y2);
    segment->segment.point_count        += 2;
    segment->segment.points_rand_access = NULL;
    
    segment->segment.start              = segment->segment.points_ll;
    segment->segment.end                = segment->segment.points_ll->next;
        
    segment->segment.curr_point         = segment->segment.end;
    
    return segment;    
}

int counterclockwise(Point *p0, Point *p1, Point *p2) 
{    // returns 1 if going counterclockwise -1 going clockwise 0 if they touch     
  if ((p1->y - p0->y) * (p2->x - p0->x) < (p2->y - p0->y) * (p1->x - p0->x))        return 1;
  if ((p1->y - p0->y) * (p2->x - p0->x) > (p2->y - p0->y) * (p1->x - p0->x))        return -1;
  if ((p1->x - p0->x) * (p2->x - p0->x) < 0 || (p1->y - p0->y)*(p2->y - p0->y) < 0) return -1;
  
  if ((sqrt(p1->x - p0->x) + sqrt(p1->y - p0->y)) >= (sqrt(p2->x - p0->x) + sqrt(p2->y - p0->y)))
    return 0;
  else
    return 1;
}

int compare_points(const void *a, const void *b) /* C's qsort comparison function for points */
{   return ((*(Point_LL **)(a))->point.x - (*(Point_LL **)(b))->point.x) ?
           ((*(Point_LL **)(a))->point.x - (*(Point_LL **)(b))->point.x) :
           ((*(Point_LL **)(b))->point.y - (*(Point_LL **)(a))->point.y) ;   
}

int compare_segments(const void *a, const void *b) /* C's qsorts comparinson function for segments */
{   return ((*(Segment_LL **)(a))->segment.start->point.x - (*(Segment_LL **)(b))->segment.start->point.x) ?
           ((*(Segment_LL **)(a))->segment.start->point.x - (*(Segment_LL **)(b))->segment.start->point.x) :
           ((*(Segment_LL **)(b))->segment.start->point.y - (*(Segment_LL **)(a))->segment.start->point.y) ;
}

int intersecting(Segment_LL *segment_1, Segment_LL *segment_2)
{
    Point *s1_p1 = &segment_1->segment.start->point ,
          *s1_p2 = &segment_1->segment.end->point   ,
          *s2_p1 = &segment_2->segment.start->point ,
          *s2_p2 = &segment_2->segment.end->point   ;
                
    return ((counterclockwise(s1_p1, s1_p2, s2_p1) * counterclockwise(s1_p1, s1_p2, s2_p2)) <= 0 && 
            (counterclockwise(s2_p1, s2_p2, s1_p1) * counterclockwise(s2_p1, s2_p2, s1_p2)) <= 0);
}

Point_LL *get_interception_point(Segment_LL *segment_1, Segment_LL *segment_2)
{
    double mua, mub, denom, numera, numerb;   
    
    int    x1 = segment_1->segment.start->point.x, y1 = segment_1->segment.start->point.y,
           x2 = segment_1->segment.end->point.x,   y2 = segment_1->segment.end->point.y,
           x3 = segment_2->segment.start->point.x, y3 = segment_2->segment.start->point.y,
           x4 = segment_2->segment.end->point.x,   y4 = segment_2->segment.end->point.y;

    denom  = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    numera = (x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3);
    numerb = (x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3);
        
    if (!denom) 
        error("Segments are parallel!");
    
    mua = numera / denom;
    mub = numerb / denom;
    if (mua < 0 || mua > 1 || mub < 0 || mub > 1) 
        error("Lines do not intersect!");
        
    if (!abs(numera) && !abs(numerb) && !abs(denom))     
        return get_new_point_ll(((x1 + x2) / 2), ((y1 + y2) / 2), NULL, NULL);
                
    return get_new_point_ll((x1 + mua * (x2 - x1)), (y1 + mua * (y2 - y1)), NULL, NULL);
}

/* Sort all the points within a segment from left to right, update both LL and pointer table ...*/
void sort_segment_points(Segment_LL *segment, unsigned int segment_index)
{
    Point_LL *curr_point = segment->segment.points_ll;
    segment->segment.points_rand_access = (Point_LL **)malloc(sizeof(Point_LL *) * segment->segment.point_count);
    unsigned int index = 0;
    for (; index < segment->segment.point_count; index++)
    {
        segment->segment.points_rand_access[index] = curr_point;
        curr_point = curr_point->next;
    }
    
    qsort(segment->segment.points_rand_access, segment->segment.point_count, sizeof(Point_LL *), compare_points);
    
    curr_point = segment->segment.points_ll = segment->segment.points_rand_access[0];    
    
    segment->segment.points_ll->prev = NULL;
    for (index = 1; index < segment->segment.point_count; index++)
    {
        curr_point->next = segment->segment.points_rand_access[index];
        curr_point->next->prev = curr_point;
        curr_point = curr_point->next;    
        
    }
    curr_point->next = NULL;
    
    if (curr_point != segment->segment.end)
        error("The end point wasn't properly calculated!");
    if (segment->segment.points_ll != segment->segment.start)
        error("The start point wasn't properly calculated!");
}

/* Sort all segments from left to write, update LL and pointer table ...*/
void sort_segments(Data *data)
{
    unsigned int index = 0;
    data->segments_rand_access = (Segment_LL **)malloc(sizeof(Segment_LL *) * data->segment_count);
    Segment_LL *curr_segment = data->segments_ll;    
    while (curr_segment)
    {
        data->segments_rand_access[index++] = curr_segment;
        curr_segment = curr_segment->next;
    }
        
    qsort(data->segments_rand_access, data->segment_count, sizeof(Point_LL *), compare_segments);         
    
    curr_segment = data->segments_ll = data->segments_rand_access[0];
    for (index = 1; index < data->segment_count; index++)
    {
        curr_segment->next = data->segments_rand_access[index];
        curr_segment = curr_segment->next;
    }
    
    curr_segment->next = NULL;    
}

/* Check if two points are identical or close enough due to floating point issues ...*/
int check_points(Point *point_1, Point *point_2)
{    return (!abs(point_1->x - point_2->x) && !abs(point_1->y - point_2->y));    }

/* Set the index of an intersection point so as to be able to instantly located in the second vertex ...*/
void set_point_index(Data *data, Point_LL *point)
{
    if (point->point.int_segment_index == -1)
        error("Segment index hasn't being set!");
    
    Segment_LL *segment = data->segments_rand_access[point->point.int_segment_index];    
    
    unsigned int index = 0;
    for (; index < segment->segment.point_count; index++)
        if (check_points(&segment->segment.points_rand_access[index]->point, &point->point))
        {
            point->point.int_point_index = index;
            return ;
        }
    
    error("Couldn't find point index!");
}

/* Set all interception point indices, for constant time look ups ... */
void set_interception_point_indexs(Data *data)
{
    Segment_LL *curr_segment = data->segments_ll;
    Point_LL   *curr_point   = NULL;
    while (curr_segment)
    {
        if (curr_segment->segment.point_count > 2)
        {
            curr_point = curr_segment->segment.points_ll->next;
            while (curr_point != curr_segment->segment.end)  
            {
                set_point_index(data, curr_point);
                curr_point = curr_point->next;
            }                            
        }
        curr_segment = curr_segment->next;
    }    
}

/* Look for intersections */
void set_intersections(Data *data)
{    
    Segment_LL *curr_segment_1 = data->segments_ll, *curr_segment_2 = NULL;
    unsigned int segment_index = 0;
    
    while (curr_segment_1)
    {
        curr_segment_2 = data->segments_ll;
        segment_index = 0;
        while (curr_segment_2)
        {            
            if (curr_segment_1 != curr_segment_2 && intersecting(curr_segment_1, curr_segment_2))     
            {
                curr_segment_1->segment.curr_point->next = get_interception_point(curr_segment_1, curr_segment_2);
                curr_segment_1->segment.curr_point = curr_segment_1->segment.curr_point->next;
                curr_segment_1->segment.curr_point->point.int_segment_index = segment_index;
                curr_segment_1->segment.point_count++; 
                                
            }
            curr_segment_2 = curr_segment_2->next;
            segment_index++;
        }               
        sort_segment_points(curr_segment_1, segment_index);        
        curr_segment_1 = curr_segment_1->next;        
    }
    
    set_interception_point_indexs(data);
}

/* Read data from the file, get interval, segments and intersections ... return data*/
Data *get_data_from_file(FILE *file) 
{
    if (!file) error("File object is required to retrieve data!");
    
    Data *data = (Data *)malloc(sizeof(Data));
        
    data->segments_ll             = NULL;
    data->segments_rand_access    = NULL;
    data->segment_count           = 0;
    
    data->all_points              = NULL;
    data->points_count            = 0;
        
    if (fscanf(file, "[%d, %d]x[%d,%d]\n", &data->xintv.min, &data->xintv.max, &data->yintv.min, &data->yintv.max) != 4)
        error("Failed to read the initial 4 values that set the intervals");
    
    int x1, y1, x2, y2;            
    Segment_LL *curr_segment = NULL;
    
    if (fscanf(file, "S (%d, %d) (%d, %d)\n", &x1, &y1, &x2, &y2) == 4)
    {        
        curr_segment = data->segments_ll = get_new_segment_ll(x1, y1, x2, y2);
        data->segment_count++;
    }
    
    while (fscanf(file, "S (%d, %d) (%d, %d)\n", &x1, &y1, &x2, &y2) == 4)
    {        
        curr_segment->next = get_new_segment_ll(x1, y1, x2, y2);
        curr_segment = curr_segment->next;
        data->segment_count++;
    } 
        
    sort_segments(data);    
    set_intersections(data);
    
    return data;        
}

/* get sweep path, by getting and sorting all points from left to right ...*/
void set_sweep_path(Data *data)
{
    
    unsigned int point_count = 0, segment_index = 0, point_index = 0;
    Segment_LL *curr_segment = data->segments_ll;
    while (curr_segment)
    {
        point_count += curr_segment->segment.point_count;
        curr_segment = curr_segment->next;
    }
    
    Point_LL **all_points = (Point_LL **)malloc(sizeof(Point_LL *) * point_count);
    curr_segment = data->segments_ll;
    point_count = 0;
    for (segment_index = 0; segment_index < data->segment_count; segment_index++)
        for (point_index = 0; point_index < data->segments_rand_access[segment_index]->segment.point_count; point_index++)
            all_points[point_count++] = data->segments_rand_access[segment_index]->segment.points_rand_access[point_index];
        
    qsort(all_points, point_count, sizeof(Point_LL *), compare_points);
    
    data->all_points = all_points;    
    data->points_count = point_count;
}

/* Distance so far from point_1 to point_2 */
double get_distance(Point_LL *point_1, Point_LL *point_2)
{
    if (point_1->point.distance == -1)
        error("Distance hasn't being properly set!");
        
    return  point_1->point.distance + 
            sqrt(pow((point_2->point.x - point_1->point.x), 2) + pow((point_2->point.y - point_1->point.y), 2));
}

/* Sweep along all the points, and set their distances and prev left most connected point */
void sweep(Data *data)
{
    set_sweep_path(data);
    double distance_1, distance_2;
    unsigned int int_segment, int_point, index = 0;
           
    for (index = 0; index < data->points_count; index++)
    {     
            
        if (!data->all_points[index]->prev && data->all_points[index]->next) // Starting point 
        {
            data->all_points[index]->point.distance = 0;
            data->all_points[index]->point.prev = NULL;
        }
        else if (data->all_points[index]->prev && !data->all_points[index]->next) // Ending point
        {
            data->all_points[index]->point.distance = get_distance(data->all_points[index]->prev, data->all_points[index]);
            data->all_points[index]->point.prev = data->all_points[index]->prev;
        }
        else if (data->all_points[index]->prev && data->all_points[index]->next)
        {            
            int_segment = data->all_points[index]->point.int_segment_index;
            int_point   = data->all_points[index]->point.int_point_index;
            
            distance_1  = get_distance(data->all_points[index]->prev, 
                                       data->all_points[index]);            
            
            distance_2  = get_distance(data->segments_rand_access[int_segment]->segment.points_rand_access[int_point]->prev, 
                                       data->all_points[index]);
            
            if (distance_1 > distance_2)
            {
                data->all_points[index]->point.distance = distance_1;
                data->all_points[index]->point.prev     = data->all_points[index]->prev;
            }
            else
            {
                data->all_points[index]->point.distance = distance_2;
                data->all_points[index]->point.prev     = data->segments_rand_access[int_segment]->segment.points_rand_access[int_point]->prev;
            }
        }
        else
            error("Bad sweep!");            
    }    
}

Point_LL *get_point_max_distance(Data *data)
{
    if (!data->all_points)
        return NULL;
    
    Point_LL *max = data->all_points[0];
    unsigned int index = 0;
    for (index = 0; index < data->points_count; index++)
        if (data->all_points[index]->point.distance > max->point.distance)
            max = data->all_points[index];
    return max;
}
/* Get path by working backwards from right most point */
void set_path(Data *data)
{
    
    Point_LL *curr = get_point_max_distance(data);
    if (!curr) return ;
    
    Point_LL *path = get_new_point_ll(curr->point.x, curr->point.y, NULL, NULL), *prev = NULL;    
    path->point.distance = curr->point.distance;
    curr = curr->prev;
    while (curr)
    {                
        prev = path;
        path = get_new_point_ll(curr->point.x, curr->point.y, NULL, NULL);
        path->point.distance = curr->point.distance;
        path->next = prev;
        curr = curr->point.prev;
    }
    
    data->path = path;   
}
 
/* Display path ...*/
void display_path(Data *data, int argc, char **argv)
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
    sprintf(win_name_string, "---> Max Dist(%f)", data->all_points[data->points_count - 1]->point.distance);        
    
    char *icon_name_string = "Icon for Window";    

    XTextProperty win_name, icon_name;
        
    GC gc, gc_blue, gc_red, curr_gc, gc_white;
    unsigned long valuemask = 0;
    XGCValues gc_values, gc_blue_values, gc_red_values, gc_white_values;
    
    XColor tmp_color1, tmp_color2;
    
    if ((display_ptr = XOpenDisplay(display_name)) == NULL)
        error("Could not open display. \n"); 

    screen_num          = DefaultScreen         (display_ptr);
    screen_ptr          = DefaultScreenOfDisplay(display_ptr);
    color_map           = XDefaultColormap      (display_ptr, screen_num);
    display_width       = DisplayWidth          (display_ptr, screen_num);
    display_height      = DisplayHeight         (display_ptr, screen_num);

    /* creating the window */
    
    
    border_width = 10;
    win_x        = (display_width/2)  - (abs(data->xintv.max - data->xintv.min)+ border_width*2)/2; 
    win_y        = (display_height/2) - (abs(data->yintv.max - data->yintv.min)+ border_width*2)/2;
        
    win_width    = abs(data->xintv.max - data->xintv.min) + border_width*2;
    win_height   = abs(data->yintv.max - data->yintv.min) + border_width*2; /*rectangular window*/
    

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
  
    gc_red = XCreateGC( display_ptr, win, valuemask, &gc_red_values);
    XSetLineAttributes( display_ptr, gc_red, 3, LineSolid, CapButt, JoinRound);
    if (XAllocNamedColor( display_ptr, color_map, "red", &tmp_color1, &tmp_color2) == 0)
        error("failed to get color red\n"); 
    else
        XSetForeground(display_ptr, gc_red, tmp_color1.pixel);
      
    gc_blue = XCreateGC(display_ptr, win, valuemask, &gc_blue_values);
    XSetLineAttributes (display_ptr, gc_blue, 1, LineSolid, CapRound, JoinRound);
    if (!XAllocNamedColor(display_ptr, color_map, "blue", &tmp_color1, &tmp_color2))
        error("failed to get color blue\n");     
    else
        XSetForeground(display_ptr, gc_blue, tmp_color1.pixel);
    
    gc_white = XCreateGC(display_ptr, win, valuemask, &gc_white_values);
    XSetLineAttributes (display_ptr, gc_white, 1, LineSolid, CapButt , JoinBevel);
    if (!XAllocNamedColor(display_ptr, color_map, "white", &tmp_color1, &tmp_color2))
        error("failed to get color white\n");     
    else
        XSetForeground(display_ptr, gc_white, tmp_color1.pixel);                    
    
    int xoffset = abs(data->xintv.min) + 5, yoffset = abs(data->yintv.min) + 5;
    curr_gc = gc_blue;
    
    Segment_LL *curr_segment = data->segments_ll;
    Point_LL *curr_path = data->path, *curr_point = NULL, *p1 = NULL, *p2 = NULL;
    while (1)
    { 
        XNextEvent( display_ptr, &report );
        switch (report.type)
	{
            case Expose:                                      
                curr_segment = data->segments_ll;
                while (curr_segment)
                {
                    XDrawLine(display_ptr, win, gc_blue, 
                        curr_segment->segment.start->point.x + xoffset, curr_segment->segment.start->point.y + yoffset,
                        curr_segment->segment.end->point.x   + xoffset, curr_segment->segment.end->point.y   + yoffset);
                    
                    curr_point = curr_segment->segment.points_ll;
                    while (curr_point)
                    {
                        XFillArc(display_ptr, win, gc,                             
                            curr_point->point.x + xoffset, curr_point->point.y + yoffset,
                            4, 4, 
                            0, 360*64);
                        curr_point = curr_point->next;
                        
                    }
                    curr_segment = (Segment_LL *)curr_segment->next;
                }
                                
                
                curr_path = data->path;
                while (curr_path->next)
                {                    
                    p1 = curr_path;                    
                    p2 = curr_path->next;
                                        
                    XDrawLine(display_ptr, win, gc_red,
                        p1->point.x + xoffset, p1->point.y + yoffset, 
                        p2->point.x + xoffset, p2->point.y + yoffset);                    
                    curr_path = curr_path->next;
                }
                
                break;
                
            case ButtonPressMask:
                
                if (report.xbutton.button == Button1 && curr_path->next)
                {
                    p1 = curr_path;                    
                    p2 = curr_path->next;                    
                    XDrawLine(display_ptr, win, gc_red,
                        p1->point.x + xoffset, p1->point.y + yoffset, 
                        p2->point.x + xoffset, p2->point.y + yoffset);                                                        
                    curr_path = curr_path->next;
                }                                
                else if (report.xbutton.button != Button1)
                {
                    curr_path = data->path;
                    while (curr_path->next)
                    {                    
                        p1 = curr_path;                        
                        p2 = curr_path->next;
                        XDrawLine(display_ptr, win, gc_blue,
                            p1->point.x + xoffset, p1->point.y + yoffset, 
                            p2->point.x + xoffset, p2->point.y + yoffset);                    
                        curr_path = curr_path->next;                        
                    }                    
                    
                    curr_path = data->path;
                }                
                 
                
                break;
                
            case ConfigureNotify:          
              win_width = report.xconfigure.width;
              win_height = report.xconfigure.height;
              break;        
            default: break;
	}
    }
}       

/* Free resources .... */
void free_resources(Data *data)
{
    free(data->all_points);
    
    Segment_LL *curr_segment = data->segments_ll;
    Point_LL *curr_point = NULL;
    while (curr_segment)
    {        
        data->segments_ll = data->segments_ll->next;
        curr_point = curr_segment->segment.points_ll;
        while (curr_point)
        {
            curr_segment->segment.points_ll = curr_segment->segment.points_ll->next;
            free(curr_point);
            curr_point = curr_segment->segment.points_ll;
        }        
        free(curr_segment->segment.points_rand_access);
        free(curr_segment);
        curr_segment = data->segments_ll;                
    }
    free(data->segments_rand_access);
    
    curr_point = data->path;
    while (curr_point)
    {
        data->path = data->path->next;
        free(curr_point);
        curr_point = data->path;
    }    
}

int main(int argc, char** argv) 
{
    if (argc == 1) 
        error("Expected a filename ...\n");
    
    FILE *file = fopen(argv[1], "r");
    
    if (!file)
    {
        printf("Couldn't open %s \n", argv[1]);
        error("Unable to open file! \n");
    }
    
    Data *data = get_data_from_file(file);    
    sweep(data);
    set_path(data);    
        
    display_path(data, argc, argv);
    
    free_resources(data);
    
    return 0;
}

