#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "template.h"
#include <SDL2_gfxPrimitives.h>
#include <SDL.h>

#define WINDOW_NAME "Matcher demo"
#define TO_RAD 0.024639942381096400

#define BG_COLOR 255, 255, 255, 255
#define FG_COLOR 0, 0, 0, 255
#define HL_COLOR 255, 0, 0, 255

#define ZOOM 2

// SDL2 handles
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

void drawMinutia(T *temp, int i, int x0, int y0, char r, char g, char b, char a)
{
  double theta = temp->o[i] * TO_RAD;
  int x1 = x0 + ZOOM * temp->x[i];
  int y1 = y0 + ZOOM *temp->y[i];
  int x2 = x1 + ceil(5 * ZOOM * cos(theta));
  int y2 = y1 + ceil(5 * ZOOM * sin(theta));
  assert(lineRGBA(renderer, x1, y1, x2, y2, r, g, b, a) == 0);
  assert(rectangleRGBA(renderer, x1-ZOOM, y1-ZOOM, x1+ZOOM, y1+ZOOM, r, g, b, a) == 0);
}

void drawTemplate(T *temp, int x0, int y0, char r, char g, char b, char a)
{
  // Draw frame
  assert(rectangleRGBA(renderer, x0, y0, x0 + ZOOM * temp->width, y0 + ZOOM * temp->height, r, g, b, a) == 0);
  // Draw minutiae
  for (int i = 0; i < temp->nbMinutiae; i++) drawMinutia(temp, i, x0, y0, r, g, b, a);
}

void displayTemplates(T *t1, T *t2, int highlighted1, int highlighted2)
{
  assert(SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) == 0);
  assert(SDL_RenderClear(renderer) == 0);

  drawTemplate(t1, 0, 0, FG_COLOR);
  drawTemplate(t2, ZOOM * t1->width, 0, FG_COLOR);

  if (highlighted1 > -1)
    drawMinutia(t1, highlighted1, 0, 0, HL_COLOR);

  if (highlighted2 > -1)
    drawMinutia(t2, highlighted2, ZOOM * t1->width, 0, HL_COLOR);

  SDL_RenderPresent(renderer);
}


void initDisplay(T *t1, T *t2)
{
  int width = ZOOM * (t1->width + t2->width);
  int height = ZOOM * ((t1->height < t2->height) ? t2->height : t1->height);

  // Initialize SDL
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);

  // Initialize window
  window = SDL_CreateWindow(WINDOW_NAME,
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            width, height,
                            SDL_WINDOW_SHOWN);
  assert(window != NULL);

  // Initialize renderer
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  assert(renderer != NULL);

  displayTemplates(t1, t2, -1, -1);    
}

void destroyDisplay()
{
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void removeMinutia(T *temp, int minutiaNo)
{  
  temp->x[minutiaNo] = temp->x[temp->nbMinutiae - 1];
  temp->y[minutiaNo] = temp->y[temp->nbMinutiae - 1];
  temp->o[minutiaNo] = temp->o[temp->nbMinutiae - 1];
  temp->t[minutiaNo] = temp->t[temp->nbMinutiae - 1];
  temp->q[minutiaNo] = temp->q[temp->nbMinutiae - 1];
  temp->nbMinutiae--;
}

void selectMinutia(int x, int y, T *t1, int *select1, T *t2, int *select2)
{
  x /= ZOOM;
  y /= ZOOM;
  // assert(t1 != NULL && t2 != NULL);
  T *t = t1;
  int *select = select1;
  if (x >= t1->width)
  {
    t = t2;
    x -= t1->width;
    select = select2;
  }
  
  int d, minD = INT_MAX;
  *select = -1;
  for (int i = 0; i < t->nbMinutiae; i++)
  {
    d = (t->x[i] - x)*(t->x[i] - x) + (t->y[i] - y)*(t->y[i] - y);
    *select = (d < minD) ? i : *select;
    minD = (d < minD) ? d : minD; 
  }

  displayTemplates(t1, t2, *select1, *select2);
}

void confirm(int *nbPairs, int *pairs, T *t1, int *select1, T *t2, int *select2)
{
  if (*select1 != -1 && *select2 != -1)
  {
    pairs[2*(*nbPairs)] = *select1;
    pairs[2*(*nbPairs) + 1] = *select2;
    (*nbPairs)++;
    removeMinutia(t1, *select1);
    removeMinutia(t2, *select2);
    *select1 = *select2 = -1;
    displayTemplates(t1, t2, *select1, *select2);
  }
}

void save(int nbPairs, int *pairs, const char *templateFile1, const char *templateFile2, const char *outputFile)
{
  FILE *f = fopen(outputFile, "w");
  assert(f != NULL);

  assert(fprintf(f, "%s\n%s\n%d\n", templateFile1, templateFile2, nbPairs) > 0);

  for (int i = 0; i < nbPairs; i++)
  {
    assert(fprintf(f, "%d %d\n", pairs[2*i], pairs[2*i + 1]) > 0);
  }
  fclose(f);
}




int main(int argc, char **argv)
{
  if (argc != 4 && argc != 3)
  {
    printf("Usage: %s templateFile1 templateFile2 outputFile\n", argv[0]);
    return EXIT_FAILURE;
  }

  T temp1, temp2;
  assert(T_load(&temp1, argv[1]) == 0);
  assert(T_load(&temp2, argv[2]) == 0);

  SDL_Event event;
  initDisplay(&temp1, &temp2);
  atexit(destroyDisplay);

  int select1 = -1, select2 = -1;

  int nbPairs = 0;
  int maxNbPairs = temp1.nbMinutiae < temp2.nbMinutiae ? temp1.nbMinutiae : temp2.nbMinutiae;
  int pairs[2*maxNbPairs];

  int stop = 0;

  while(!stop)
  {
    fflush(stderr);
    SDL_WaitEvent(&event);
    switch(event.type)
    {
      case SDL_QUIT:
        stop = 1;
        break;
      
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) 
        {
          case SDLK_ESCAPE:
            stop = 1;
            break;
          
          case SDLK_KP_ENTER:
          case SDLK_RETURN:
          case SDLK_KP_SPACE:
          case SDLK_c:
            confirm(&nbPairs, pairs, &temp1, &select1, &temp2, &select2);
            break;
        }
        break;

      case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT)
          selectMinutia(event.button.x, event.button.y, &temp1, &select1, &temp2, &select2);
        break;
    }
  }

  if (argc == 4)
    save(nbPairs, pairs, argv[1], argv[2], argv[3]);

  T_free(&temp1);
  T_free(&temp2);

  return EXIT_SUCCESS;
}
