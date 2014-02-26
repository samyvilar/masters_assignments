/* 
 * File:   main.cpp
 * Author: Samy Vilar
 *
 * Created on October 7, 2011, 3:07 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>


/* General Utility stuff .... */
void error(char msg[])
{
    printf("Error: %s", msg);
    exit(-1);
}

typedef struct Element
{
    int value;
    struct Element *next;
    
} Element;

typedef struct
{    
    Element *root, *curr;    
    unsigned int count;    
} Queue;

Queue *get_new_queue()
{
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    queue->curr = queue->root = NULL;
    queue->count = 0;
    return queue;
}
void enque(Queue *queue, int value)
{
    if (!queue->root)
    {
        queue->root = (Element *)malloc(sizeof(Element));
        queue->root->value = value;
        queue->root->next = NULL;
        queue->curr = queue->root;
    }
    else
    {    
        queue->curr->next = (Element *)malloc(sizeof(Element));
        queue->curr = queue->curr->next;
        queue->curr->value = value;
        queue->curr->next = NULL;        
    }
    
    queue->count = queue->count + 1;
}

int deque(Queue *queue)
{
    if (queue->count == 0)
        return -1; /* end of queue */
    else
    {
        Element *temp = queue->root;
        queue->root = queue->root->next;
        int value = temp->value;
        free(temp);
        queue->count = queue->count - 1;
        
        if (queue->count == 0)
            queue->root = queue->curr = NULL;        
        return value;        
    }
}

void delete_queue(Queue *queue)
{
    if (!queue ) return ;    
    if (!queue->count) 
    {
        free(queue);
        return ;
    }
    
    Element *temp;
    
    while (queue->root)
    {
        temp = queue->root->next;
        free(queue->root);
        queue->root = temp;
    }
    free(queue);
}
/**************************************************************************/

typedef struct { int   x,   y;    } Point;    
typedef struct { int   min, max;  } Interval; 
typedef struct { Point p1,  p2;   } Segment; 

typedef struct Multiple_Segments
{
    Segment segment;
    struct Multiple_Segments *next;
} Multiple_Segments;  

typedef struct Multiple_Points
{
    Point point;    
    double *distances;
    struct Multiple_Points *next;
}Multiple_Points;

typedef struct  /* General Data structure to keep together all required data */
{               /* easy to pass around, doesn't pollute the global namespace */
    Multiple_Points     *points;          
    Multiple_Points     **point_array;
    int                 point_count;
    Multiple_Segments   *segments;
    int                 segment_count;
    Interval            xintv;
    Interval            yintv;    
} Data;

Data *get_data_from_file(FILE *file) 
{       /* Read Data from file, build Data object, return pointer to it */
    if (!file)
        error("File object is required to retrieve data!");
    
    Data *data = (Data *)malloc(sizeof(Data));
    
    data->point_count   = data->segment_count = 0;     
    data->segments      = NULL;
    data->points        = NULL;
    
    int count;
    if ((count = fscanf(file, "[%d, %d]x[%d,%d]\n", &data->xintv.min, &data->xintv.max, &data->yintv.min, &data->yintv.max)) != 4)
        error("Failed to read the initial 4 values that set the intervals");
    
    int x, y; 
    Multiple_Points *curr_point = NULL;
    while (fscanf(file, "P (%d, %d)\n", &x, &y) == 2)
    {
        if (!data->points)        
            data->points = curr_point = (Multiple_Points *)malloc(sizeof(Multiple_Points));
        else
        {
            curr_point->next = (Multiple_Points *)malloc(sizeof(Multiple_Points));
            curr_point = curr_point->next;            
        }
                        
        curr_point->point.x = x;
        curr_point->point.y = y;
        curr_point->next    = NULL;
        data->point_count++;
    }
    
    int x1, y1, x2, y2;
    Multiple_Segments *curr_segment = NULL;
    while (fscanf(file, "S (%d, %d) (%d, %d)\n", &x1, &y1, &x2, &y2) == 4)
    {
        if (!data->segments)
            data->segments = curr_segment = (Multiple_Segments *)malloc(sizeof(Multiple_Segments));
        else
        {                    
            curr_segment->next = malloc(sizeof(Multiple_Segments));
            curr_segment = (Multiple_Segments *)curr_segment->next;
        }
        
        curr_segment->segment.p1.x = x1;
        curr_segment->segment.p1.y = y1;
        curr_segment->segment.p2.x = x2;
        curr_segment->segment.p2.y = y2;
        curr_segment->next         = NULL;
        data->segment_count++;
    }        
    
    /* Address Table of points for fast random access ... not req but nice to have */
    Multiple_Points **point_array = (Multiple_Points **)malloc(sizeof(Multiple_Points *) * data->point_count);
    Multiple_Points *curr = data->points;
    int index = 0;
    while (curr != NULL)
    {
        point_array[index++] = curr;
        curr = (Multiple_Points *)curr->next;
    }
    
    data->point_array = point_array;
    return data;                 
}

int counterclockwise(Point *p0, Point *p1, Point *p2) 
{    /* returns 1 if going counterclockwise -1 going clockwise 0 if they touch */
  if ((p1->y - p0->y) * (p2->x - p0->x) < (p2->y - p0->y) * (p1->x - p0->x))        return 1;
  if ((p1->y - p0->y) * (p2->x - p0->x) > (p2->y - p0->y) * (p1->x - p0->x))        return -1;
  if ((p1->x - p0->x) * (p2->x - p0->x) < 0 || (p1->y - p0->y)*(p2->y - p0->y) < 0) return -1;
  
  if ((sqrt(p1->x - p0->x) + sqrt(p1->y - p0->y)) >= (sqrt(p2->x - p0->x) + sqrt(p2->y - p0->y)))
    return 0;
  else
    return 1;
}

int intercepting(Point *p1, Point *p2, Multiple_Segments *segments)
{       /* returns 1 if the segment from p1 to p2 intercepts any of other given statements */
    while (segments)
    {
        if (counterclockwise(p1, p2, &segments->segment.p1) * counterclockwise(p1, p2, &segments->segment.p2) <= 0 && 
            counterclockwise(&segments->segment.p1, &segments->segment.p2, p1)*counterclockwise(&segments->segment.p1, &segments->segment.p2, p2) <= 0)
            return 1;
            
        segments = (Multiple_Segments *)segments->next;
    }
    
    return 0;        
}

double get_distance(Point *p1, Point *p2, Multiple_Segments *segments)
{       /* returns 0 if points are the same, -1 if they intercept any segment or the distance */
    if (p1->x == p2->x && p1->y == p2->y)  return 0;      
    if (intercepting(p1, p2, segments))    return -1;
    return sqrt((p2->x - p1->x)*(p2->x - p1->x) + (p2->y - p1->y)*(p2->y - p1->y));            
}

int compare_doubles (const void *a, const void *b)
{       /* auxillay comparison function for C qsort impl */
    const double *da = (const double *) a;
    const double *db = (const double *) b;

    return (*da > *db) - (*da < *db);
}

/* Sets the distances at each vertex to all the other vertices that aren't blocked
 * by an obstacle and returns all the non-duplicating distances sorted ...
 */
double *set_distances(Data *data)
{
    int index = 0;    
    Multiple_Points *curr_point = NULL, *points = data->points; 
            
    /* the last array will be used to stored non-duplicate distances 
     * with its first element being the number of entries kept */
    double **distances = (double **)malloc(sizeof(double *) * (data->point_count + 1));  
    
    for (; index < data->point_count; index++)        
        distances[index] = (double *)malloc(sizeof(double) * (data->point_count)); /* we may only need half but ... */   
    
    int size = sizeof(double) * (((data->point_count * (data->point_count + 1))/2) + 1);
    distances[data->point_count] = malloc(size);
    memset(distances[data->point_count], 0, size);        
    
    curr_point = points = data->points;     
    int distance_index = 1, point_count = index = 0;    
    while (curr_point)
    {                        
        while (points)
        {                
           distances[point_count][index] = get_distance(&curr_point->point, &points->point, data->segments); 
           distances[index][point_count] = distances[point_count][index]; /* symmetry */ 
         
           if (distances[point_count][index] != 0 && distances[point_count][index] != -1)           
               distances[data->point_count][distance_index++] = distances[point_count][index];               
                       
           points = (Multiple_Points *)points->next;
           index++;
        }
                
        curr_point->distances = distances[point_count]; /* set reference of all calculated distances to vertex */
        curr_point = points = (Multiple_Points *)curr_point->next; /* symmetry */
        point_count++;
        index = point_count;                
    }
            
    distances[data->point_count][0] = distance_index;    /* set count and sort distances ...*/
    qsort(&distances[data->point_count][1], (distance_index - 1), sizeof(double), compare_doubles);        
    double *temp = distances[data->point_count];
    free(distances);
    return temp;
}

void set_path(int index, int *prev, Queue *queue) 
{  /* set path recursively looking back from target back to source */     
    if (!index) 
    {
        enque(queue, index);
        return;
    }
    set_path(prev[index], prev, queue);
    enque(queue, index); 
}
 
int *bfs(Data *data, double max_distance)
{    /* Apply BFS with a max_distance return path as queue or NULL if no path found ... */
   int index = 0, curr;    
        
   Queue *queue = get_new_queue();   
   enque(queue, 0);
   
   int *status = (int *)malloc(sizeof(int) * data->point_count);
   memset(status, 0, sizeof(int) * data->point_count);
     
   int *prev    = (int *)malloc(sizeof(int) * data->point_count);
   memset(prev,    0, sizeof(int) * data->point_count);      
   
    while (queue->count)
    {
        curr = deque(queue);              
                        
        if (curr == 1)
        {            
            delete_queue(queue);
            free(status);                        
            
            return prev;
        }
                
        for (index = 0; index < data->point_count; index++)
            if (data->point_array[curr]->distances[index] != -1          && 
                data->point_array[curr]->distances[index] != 0           &&    
                data->point_array[curr]->distances[index] < max_distance && 
                status[index] == 0)                
            {                
                enque(queue, index);                        
                status[index] = 1;   
                prev[index] = curr;
            }
        status[curr] = 2;
    }   
   
    delete_queue(queue);
    free(status);
    free(prev);
       
    return NULL;    
}

Queue *get_bottleneck_path(Data *data, double *distances, double *max_distance)
{    /* return Bottleneck path as Queue using bin search, set max_distance if path found  */
    int index = (int)(distances[0]/2), left = 1, right = (int)distances[0];    
    int *path = NULL;
    Queue *qpath = NULL;
    
    while (left != right && (left + 1) != right)
    {
        index = (left + right)/2;
        path = bfs(data, distances[index]);
        if (path) 
            right = index;                                            
        else                 
            left = index;            
        
        if ((left != right && (left + 1) != right))
           free(path);
    }
    
    if (path)
    {
        qpath = get_new_queue();
        set_path(1, path, qpath);
        
        if (max_distance)
            *max_distance = distances[index];
            
        return qpath;               
    }        
        
    return NULL;
}

void draw(Data *data, Queue *path, double *max_distance, int argc, char **argv)
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
    sprintf(win_name_string, ")( Path Points(%i) Obst(%i) Max Dist(%f)", data->point_count, data->segment_count, *max_distance);        
    
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
    XSetLineAttributes( display_ptr, gc_red, 1, LineSolid, CapButt, JoinRound);
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
        
    Multiple_Points     *curr_point     = NULL, *p1 = NULL, *p2 = NULL;
    Multiple_Segments   *curr_segment   = NULL;
    Element             *curr_path      = path->root;    
    
    int xoffset = abs(data->xintv.min) + 5, yoffset = abs(data->yintv.min) + 5;
    curr_gc = gc_blue;
    while (1)
    { 
        XNextEvent( display_ptr, &report );
        switch (report.type)
	{
            case Expose:              
                curr_point = data->points;
                XFillArc(display_ptr, win, gc_blue,                             
                            curr_point->point.x + xoffset, curr_point->point.y + yoffset,
                            4, 4, 
                            0, 360*64);
                curr_point = curr_point->next;
                XFillArc(display_ptr, win, gc_red,                             
                            curr_point->point.x + xoffset, curr_point->point.y + yoffset,
                            4, 4, 
                            0, 360*64);
                while (curr_point)
                {                 
                    XFillArc(display_ptr, win, gc,                             
                            curr_point->point.x + xoffset, curr_point->point.y + yoffset,
                            2, 2, 
                            0, 360*64);
                    curr_point = (Multiple_Points *)curr_point->next;
                }
                
                curr_segment = data->segments;
                while (curr_segment)
                {
                    XDrawLine(display_ptr, win, gc_red, 
                        curr_segment->segment.p1.x + xoffset, curr_segment->segment.p1.y + yoffset,
                        curr_segment->segment.p2.x + xoffset, curr_segment->segment.p2.y + yoffset);
                    curr_segment = (Multiple_Segments *)curr_segment->next;
                }
                
                
                curr_path = path->root;
                while (curr_path->next)
                {                    
                    p1 = data->point_array[curr_path->value];
                    curr_path = (Element *)curr_path->next;                    
                    p2 = data->point_array[curr_path->value];                    
                    XDrawLine(display_ptr, win, gc_blue,
                        p1->point.x + xoffset, p1->point.y + yoffset, 
                        p2->point.x + xoffset, p2->point.y + yoffset);                    
                }
                
                break;
                
            case ButtonPressMask:
                
                if (report.xbutton.button == Button1 && curr_path->next)
                {
                    p1 = data->point_array[curr_path->value];
                    curr_path = (Element *)curr_path->next;                    
                    p2 = data->point_array[curr_path->value];                    
                    XDrawLine(display_ptr, win, curr_gc,
                        p1->point.x + xoffset, p1->point.y + yoffset, 
                        p2->point.x + xoffset, p2->point.y + yoffset);                                                        
                }                                
                else if (report.xbutton.button != Button1)
                {
                    curr_path = path->root;
                    while (curr_path->next)
                    {                    
                        p1 = data->point_array[curr_path->value];
                        curr_path = (Element *)curr_path->next;                    
                        p2 = data->point_array[curr_path->value];                    
                        XDrawLine(display_ptr, win, gc_white,
                            p1->point.x + xoffset, p1->point.y + yoffset, 
                            p2->point.x + xoffset, p2->point.y + yoffset);                    
                        
                        XFillArc(display_ptr, win, gc,                             
                            p1->point.x + xoffset, p1->point.y + yoffset,
                            2, 2, 
                            0, 360*64);
                    }
                    XFillArc(display_ptr, win, gc,                             
                            p2->point.x + xoffset, p2->point.y + yoffset,
                            2, 2, 
                            0, 360*64);
                    
                    curr_path = path->root;                    
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

int main(int argc, char** argv) 
{
    if (argc == 1) error("Expected a filename ...\n");
    
    FILE *file = fopen(argv[1], "r");
    
    if (!file)
    {
        printf("Couldn't open %s \n", argv[1]);
        error("Unable to open file! \n");
    }
                            
    Data *data = get_data_from_file(file);    
    fclose(file);            
       
    double *distances = set_distances(data);    
    
    double *max_distance = (double *)malloc(sizeof(double));
    Queue *path = get_bottleneck_path(data, distances, max_distance);            
       
    draw(data, path, max_distance, argc, argv);                            
    
    /* Deallocation of resources */
    free(max_distance);
    delete_queue(path);
    Multiple_Points *curr_point = data->points;
    while (curr_point)
    {        
        curr_point = (Multiple_Points *)curr_point->next;
        free(data->points->distances);
        free(data->points);
        data->points = curr_point;                
    }    
    free(data->point_array);
    free(distances);
    
    Multiple_Segments *curr_segment = data->segments;
    while (curr_segment)
    {
        curr_segment = (Multiple_Segments *)curr_segment->next;
        free(data->segments);
        data->segments = curr_segment;
    }
    
    free(data);
    /********************************************************/
                          
    return 0;
}


