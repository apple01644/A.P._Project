import pygame, time, random, threading, time
from pygame.locals import *
from math import pi, atan2, sin ,cos, sqrt
import numpy as np
#import tensorflow as tf
#import pandas as pd
import os.path


class Block:
    def __init__(self, type, num):
        self.type = type
        self.num = num
        self.lastHitId = 0
        self.lastHitTime = -1
class Ball:
    def __init__(self, x, y, vx, vy, id):
        self.x = x
        self.y = y
        len = (vx ** 2 + vy ** 2) ** 0.5
        self.vx = vx / len
        self.vy = vy / len
        self.id = id

class SBBGame:
    def __init__(self):
        self.Window = {'width' : 576, 'height' : 800}
        self.user = True
        self.ui = False
        self.custom_state = False
        self.file = None

        self.Game = {'width' : 6, 'height' : 9}
        self.attempt = 0
        self.last_score = 0
        self.score = 0
    
    def assess_func(self, data):
        score = 0

        for log in data['Log']:
            if log[0] == '!': # hit Ball
                x = int(log[1:log.find('/')])
                y = int(log[log.find('/')+1:log.find(':')])
                score += 0.75
            else: #hit Block
                x = int(log[:log.find('/')])
                y = int(log[log.find('/')+1:log.find(':')])
                t = int(log[log.find(':')+1:])
                if t == 0:
                    score += 1 * (y / 9) / self.Data['Number of Balls']
        died = 0
        
        for x in range(self.Game['width']):   
            block = self.Game['Map'][x][self.Game['height'] - 2]
            if block:
               if block.type == 'block':
                    died = 1

        return {'score' : score, 'died' : died}

    def log_begin(self):
        self.Data = {}
        
        self.Data['Number of Balls'] = self.Game['Number of Balls']
        self.Data['Shoot Position'] = self.Game['Shoot Position']
        self.Data['Log'] = []
        self.Data['Map'] = [['' for y in range(self.Game['height'])] for x in range(self.Game['width'])]
        for y in range(self.Game['height']):
            for x in range(self.Game['width']):
                block = self.Game['Map'][x][y]
                if block:
                    if block.type == 'block':
                        self.Data['Map'][x][y] = 'block:' + str(block.num)
                    elif block.type == 'ball':
                        self.Data['Map'][x][y] = 'ball'
                    else:
                        print('[SBBGame]Wrong type of block')
                        zero = 0
                        one = 1
                        error = one / zero

    def get_result(self):
        result = self.assess_func(self.Data)
        score = result['score']
        died = result['died']
        state = {}

        state['num_map'] = []
        state['type_map'] = []
        state['pos'] = self.Data['Shoot Position']

        for y in range(self.Game['height']):
            _ = []
            for x in range(self.Game['width']):
                if self.Data['Map'][x][y].startswith('block:'):
                    _.append(int(self.Data['Map'][x][y][self.Data['Map'][x][y].find(':') + 1:]) / self.Data['Number of Balls'])
                elif self.Data['Map'][x][y] == 'ball':
                    _.append(5)
                else:
                    _.append(0)
            state['num_map'].append(_)
            
        for y in range(self.Game['height']):
            _ = []
            for x in range(self.Game['width']):
                if self.Data['Map'][x][y].startswith('block:'):
                    _.append(1)
                elif self.Data['Map'][x][y] == 'ball':
                    _.append(-1)
                else:
                    _.append(0)
            state['type_map'].append(_)

        return [score, died, state]
    
    def toData(self):
        
        state = {}
        state['x'] = self.Data['Shoot Position']['x'] / 100
        state['balls'] = self.Data['Number of Balls']
        state['num_map'] = []
        state['type_map'] = []

        for y in range(self.Game['height']):
            _ = []
            for x in range(self.Game['width']):
                if self.Data['Map'][x][y].startswith('block:'):
                    _.append(int(self.Data['Map'][x][y][self.Data['Map'][x][y].find(':') + 1:]) / self.Data['Number of Balls'])
                #elif self.Data['Map'][x][y] == 'ball':
                #    _.append(1)
                else:
                    _.append(0)
            state['num_map'].append(_)
            
        for y in range(self.Game['height']):
            _ = []
            for x in range(self.Game['width']):
                if self.Data['Map'][x][y].startswith('block:'):
                    _.append(1)
                elif self.Data['Map'][x][y] == 'ball':
                    _.append(-1)
                else:
                    _.append(0)
            state['type_map'].append(_)
        return state
        
    def fromData(self, data):
        
        for x in range(self.Game['width']):   
            for y in range(self.Game['height']):
                if data['type_map'][y][x] == 1:
                    self.Game['Map'][x][y] = Block('block', data['num_map'][y][x])                    
                elif data['type_map'][y][x] == -1:
                    self.Game['Map'][x][y] = Block('ball', 1)

        self.Game['State'] = 'shoot'
        self.Game['Number of Balls'] = data['balls']
        self.Game['Shoot Position'] = {'x' : data['x'], 'y' : 100 - self.Game['Ball Radius']}
        
        self.log_begin()
        
      
    def initialize(self):
        self.Game['Map'] = np.array([[None for y in range(self.Game['height'])] for x in range(self.Game['width'])], dtype=np.object)
        
        self.Game['Shoot Balls'] = []
        self.Game['Score'] = 1
        self.Game['Number of Balls'] = 1
        
        self.Game['Shoot Position'] = {'x' : 50, 'y' : 100 - self.Game['Ball Radius']}
        self.Game['State'] = 'prepare'

    def action_Shoot(self, deg):
        
        rad = (deg * (pi - 0.3)) - pi + 0.15
        self.Game['Left Balls'] = self.Game['Number of Balls']
        self.Game['Shoot Radian'] = rad
        self.Game['Last Shoot'] = t
        self.Game['State'] = 'shooting'
        self.Game['Ground Ball'] = False
        self.Data['Shoot Degreed'] = deg

    def action_Prepare(self):
        #Generate Header
        cursor = random.randrange(0, self.Game['width'])
        self.Game['Map'][cursor][0] = Block('ball', 1)

        while True:
            cursor = random.randrange(0, self.Game['width'])
            if self.Game['Map'][cursor][0] == None:
                self.Game['Map'][cursor][0] = Block('block', self.Game['Score'])
                break

        left_ball = self.Game['Score'] // 20 + 1
        for x in range(self.Game['width']):
            if left_ball == 0:
                break
            block = self.Game['Map'][x][0]
            if block == None:
                if random.randint(0, 1) == 1:
                    self.Game['Map'][x][0] = Block('block', self.Game['Score'])
                    left_ball -= 1
        
        #Get Down
        for y_ in range(self.Game['height'] - 1):
            y = self.Game['height'] - 1 - 1 - y_
            for x in range(self.Game['width']):
                block = self.Game['Map'][x][y]
                if block:
                    self.Game['Map'][x][y + 1] = self.Game['Map'][x][y]
                    self.Game['Map'][x][y] = None

        #Check trailer    
        for x in range(self.Game['width']):   
            block = self.Game['Map'][x][self.Game['height'] - 1]
            if block:
                if block.type == 'ball':
                    self.Game['Number of Balls'] += 1
                    self.Game['Map'][x][self.Game['height'] - 1] = None
                elif block.type == 'block':
                    print('Game Over - Score : %d' % self.Game['Score'])
                    self.attempt += 1
                    self.Game['State'] = 'lose'
                    self.initialize()
                    return
        self.log_begin()
                                
        self.Game['State'] = 'shoot'
  
    def action_CleanUp(self):                  
        self.Game['Shoot Position'] = self.Game['New Shoot Position']
        self.Game['State'] = 'prepare'
        self.last_score = self.score

    def collision_between_rect_circle(self, rect_x, rect_y, rect_w, rect_h, circle_x, circle_y, circle_r) :
        rectA = {'x1' : rect_x, 'x2' : rect_x + rect_w, 'y1' : rect_y, 'y2' : rect_y + rect_h}
        rectB = {'x1' : circle_x - circle_r, 'x2' : circle_x + circle_r, 'y1' : circle_y - circle_r, 'y2' : circle_y + circle_r }
        
        if rectA['x1'] < rectB['x2'] and rectA['x2'] > rectB['x1'] and rectA['y1'] < rectB['y2'] and rectA['y2'] > rectB['y1'] :
            if (circle_y > rect_y + rect_h or circle_y < rect_y) and (circle_x > rect_x + rect_w or circle_x < rect_x):
                near = {}

                if circle_x > rect_x + rect_w / 2:
                    near['x'] = rect_x + rect_w
                else:
                    near['x'] = rect_x

                if (circle_y > rect_y + rect_h / 2):
                    near['y'] = rect_y + rect_h
                else:
                    near['y'] = rect_y

                vec = {'x' : near['x'] - circle_x, 'y' : near['y'] - circle_y}
                return sqrt(vec['x'] ** 2 + vec['y'] ** 2) <= circle_r
            else:
                return True
        return False

    def draw_rect(self, surface, rgb, objT, worldT):
        position = np.dot(np.array((objT[0], objT[1],1), dtype=np.float), worldT)
        scale = np.array((objT[2] * worldT[0][0], objT[3] * worldT[1][1]), dtype=np.float)
        pygame.draw.rect(surface, rgb, (position[0], position[1], scale[0], scale[1]))
    def draw_line(self, surface, rgb, objTA, objTB, worldT, width = 1):
        positionA = np.dot(np.array((objTA[0], objTA[1],1), dtype=np.float), worldT)
        positionB = np.dot(np.array((objTB[0], objTB[1],1), dtype=np.float), worldT)                
        pygame.draw.line(surface, rgb, (positionA[0], positionA[1]), (positionB[0], positionB[1]), width)                
    def draw_circle(self, surface, rgb, objT, worldT):
        position = np.dot(np.array((objT[0],objT[1],1), dtype=np.float), worldT)
        scale = np.array((objT[2] * worldT[0][0], objT[2] * worldT[1][1]), dtype=np.float)
        pygame.draw.circle(surface, rgb, (int(position[0]), int(position[1])), int(scale[0]))
    def draw_text(self, surface, rgb, objT, worldT, text):
        position = np.dot(np.array((objT[0], objT[1],1), dtype=np.float), worldT)
        f = self.font.render(text, False, rgb)
        r = f.get_rect()
        surface.blit(f,(position[0] - r.w / 2,position[1] - r.h))
    
    def start(self):
        if self.ui:
            self.ui_start()
        self.Run = True

        self.Game['Ball Radius'] = 100 / self.Game['height'] * 0.185
                
        self.initialize()    

        t = 0.0

    def ui_start(self):
        self.ui = True
        pygame.init()
        pygame.font.init()
        self.font = pygame.font.SysFont('Comic Sans MS', self.Window['height'] // 36)
        self.surf = pygame.display.set_mode((self.Window['width'], self.Window['height']))

    def loop(self, t):        
        dt = 0.1
        if self.ui:
            trans = np.array(((1,0,0), (0,1,0), (0,0,1)), dtype=np.float)
            trans[2][1] = (self.Window['height'] - self.Window['width']) / 2
            trans[0][0] = self.Window['width'] / 100
            trans[1][1] = trans[0][0]
            
            for e in pygame.event.get():
                if e.type == pygame.locals.QUIT:
                    self.Run = False
                    self.End = 'End by User'
                elif e.type == pygame.locals.MOUSEBUTTONUP:
                    if e.button == 1:
                        if self.Game['State'] == 'shoot' and self.user:
                            position = np.dot(np.array((e.pos[0], e.pos[1],1), dtype=np.float), np.linalg.inv(trans))
                            if 0 <= position[0] <= 100 and 0 <= position[1] <= 100:
                                vx = position[0] - self.Game['Shoot Position']['x']
                                vy = position[1] - self.Game['Shoot Position']['y']
                                
                                rad = atan2(vy, vx)
                                if -pi + 0.15 < rad < 0 - 0.15:
                                    deg = (rad + pi - 0.15) / (pi - 0.3)
                                    self.action_Shoot(deg)

            self.surf.fill((240, 240, 240))
            pixel = 1 / trans[0][0]
            self.draw_rect(self.surf, (230, 230, 230), (0, 0, 100, 100), trans)

            for y in range(self.Game['height']):
                for x in range(self.Game['width']):
                    block = self.Game['Map'][x][y]
                    if block:
                        if block.type == 'block':
                            self.draw_rect(self.surf, (208, 208 , 208), (100 / self.Game['width'] * x + pixel * 5, 100 / self.Game['height'] * y + pixel * 7, 100 / self.Game['width'] - pixel * 4, 100 / self.Game['height'] - pixel * 4), trans)
                            self.draw_rect(self.surf, (255,  65 ,  81), (100 / self.Game['width'] * x + pixel * 2, 100 / self.Game['height'] * y + pixel * 2, 100 / self.Game['width'] - pixel * 4, 100 / self.Game['height'] - pixel * 4), trans)
                            self.draw_text(self.surf, (255, 255 , 255), (100 / self.Game['width'] * x + 50 / self.Game['width'], 100 / self.Game['height'] * y + 50 / self.Game['width']), trans, str(block.num))
                        elif block.type == 'ball':
                            self.draw_circle(self.surf, ( 60, 208 ,  97), (100 / self.Game['width'] * (x + 0.5), 100 / self.Game['height'] * (y + 0.5), self.Game['Ball Radius'] * (1 + 0.75 * (sin(t / 20) + 1) / 2)), trans)
                            self.draw_circle(self.surf, (230, 230 , 230), (100 / self.Game['width'] * (x + 0.5), 100 / self.Game['height'] * (y + 0.5), self.Game['Ball Radius'] * (1 + 0.25 * (sin(t / 20) + 1) / 2)), trans)
                            self.draw_circle(self.surf, ( 60, 208 ,  97), (100 / self.Game['width'] * (x + 0.5), 100 / self.Game['height'] * (y + 0.5), self.Game['Ball Radius']), trans)
                        else:
                            self.draw_rect(self.surf, (255,   0 , 255), (100 / self.Game['width'] * x + pixel * 2, 100 / self.Game['height'] * y + pixel * 2, 100 / self.Game['width'] - pixel * 4, 100 / self.Game['height'] - pixel * 4), trans)
                            

            self.draw_rect(self.surf, (0, 0, 0), (0, - pixel * 8, 100, pixel * 8), trans)
            self.draw_rect(self.surf, (0, 0, 0), (0, 100, 100, pixel * 8), trans)
            
            self.draw_text(self.surf, (0 ,0, 0), (50, 0 + 50 / 9), trans, str(self.last_score))

            
            #rad = (self.guide_line * (pi - 0.3)) - pi + 0.15
            #self.draw_line(self.surf, (0, 0, 0), (self.Game['Shoot Position']['x'], self.Game['Shoot Position']['y']), (self.Game['Shoot Position']['x'] + 50 * cos(rad), self.Game['Shoot Position']['y'] + 50 * sin(rad)), trans, width=3)
            
            if self.Game['State'] == 'shoot':
                self.draw_circle(self.surf, ( 92, 166 , 231), (self.Game['Shoot Position']['x'], self.Game['Shoot Position']['y'], self.Game['Ball Radius']), trans)
                self.draw_text(self.surf, ( 92, 166 , 231), (self.Game['Shoot Position']['x'], self.Game['Shoot Position']['y'] - pixel * 50), trans, 'x' + str(self.Game['Number of Balls']))
            elif self.Game['State'] == 'shooting':
                if self.Game['Left Balls'] > 0:
                    self.draw_circle(self.surf, ( 92, 166 , 231), (self.Game['Shoot Position']['x'], self.Game['Shoot Position']['y'], self.Game['Ball Radius']), trans)
                    self.draw_text(self.surf, ( 92, 166 , 231), (self.Game['Shoot Position']['x'], self.Game['Shoot Position']['y'] - pixel * 50), trans, 'x' + str(self.Game['Left Balls']))
                if self.Game['Ground Ball']:
                    self.draw_circle(self.surf, ( 92, 166 , 231), (self.Game['New Shoot Position']['x'], self.Game['New Shoot Position']['y'], self.Game['Ball Radius']), trans)
            for ball in self.Game['Shoot Balls']:
                self.draw_circle(self.surf, ( 92, 166 , 231), (ball.x, ball.y, self.Game['Ball Radius']), trans)


            pygame.display.flip()
        #else: # auto Shooting
        for _____ in range(8):
            if self.Game['State'] == 'shoot' and (not self.user):
                pass

            #=========================================
            if self.Game['State'] == 'shooting':
                if self.Game['Left Balls'] > 0:
                    if t - self.Game['Last Shoot'] > 30:
                        self.Game['Shoot Balls'].append(Ball(
                            self.Game['Shoot Position']['x'], 
                            self.Game['Shoot Position']['y'], 
                            cos(self.Game['Shoot Radian']), 
                            sin(self.Game['Shoot Radian']),
                            self.Game['Left Balls']
                        ))
                        self.Game['Last Shoot'] = t
                        self.Game['Left Balls'] -= 1
                else:
                    if len(self.Game['Shoot Balls']) == 0: #End of Turn
                        self.Game['Score'] += 1
                        if self.user: 
                            self.Game['State'] = 'cleanup'
                        else:
                            self.Game['State'] = 'result'
            elif self.Game['State'] == 'prepare':
                if not self.custom_state:
                    self.action_Prepare()
            elif self.Game['State'] == 'cleanup':
                self.action_CleanUp()
            for ball in self.Game['Shoot Balls']:
                ball.x += ball.vx * dt
                ball.y += ball.vy * dt
                if ball.x > 100 - self.Game['Ball Radius']:
                    ball.x = 100 - self.Game['Ball Radius']
                    ball.vx = -abs(ball.vx)
                if ball.x < self.Game['Ball Radius']:
                    ball.x = self.Game['Ball Radius']
                    ball.vx = abs(ball.vx)
                if ball.y < self.Game['Ball Radius']:
                    ball.y = self.Game['Ball Radius']
                    ball.vy = abs(ball.vy)
                    
                for y in range(self.Game['height']):
                    for x in range(self.Game['width']):
                        block = self.Game['Map'][x][y]
                        if block:
                            if block.type == 'block':
                                if self.collision_between_rect_circle(x * 100 / 6, y * 100 / 9, 100 / 6, 100 / 9, ball.x, ball.y, self.Game['Ball Radius']):
                                    if (ball.y > (y * 100 / 9 + 1) + (100 / 9 - 2) or ball.y < (y * 100 / 9 + 1)) and (ball.x > (x * 100 / 6 + 1) + (100 / 6 - 2) or ball.x < (x * 100 / 6 + 1)):
                                        near = {}
                                        point = {'x' : 0, 'y' : 0}
                                            
                                        if ball.x > (x * 100 / 6 + 1) + (100 / 6 - 2) / 2:
                                            near['x'] = (x * 100 / 6 + 1) + (100 / 6 - 2)
                                            point['x'] = 1
                                        else:
                                            near['x'] = (x * 100 / 6 + 1)
                                            point['x'] = -1
                                            
                                        if ball.y > (y * 100 / 9 + 1) + (100 / 9 - 2) / 2:
                                            near['y'] = (y * 100 / 9 + 1) + (100 / 9 - 2)
                                            point['y'] = 1
                                        else:
                                            near['y'] = (y * 100 / 9 + 1)
                                            point['y'] = -1
                                            
                                        passed = {}
                                        for Dx in {'-1' : '-1', '1' : '1'}:
                                            passed['x' + Dx] = 0 <= (x + int(Dx)) < 6 and (self.Game['Map'][x + int(Dx)][y] == None or self.Game['Map'][x + int(Dx)][y].type != 'block')
                                        for Dy in {'-1' : '-1', '1' : '1'}:
                                            passed['y' + Dy] = 0 <= (y + int(Dy)) < 9 and (self.Game['Map'][x][y + int(Dy)] == None or self.Game['Map'][x][y + int(Dy)].type != 'block')
                                            
                                        if (passed['x' + str(point['x'])] and passed['y' + str(point['y'])]): #You hit on vertex
                                            vec = {'x' : near['x'] - ball.x, 'y' : near['y'] - ball.y}#Circle to point
                                            rad_C2P = atan2(vec['y'], vec['x']) + pi / 2
                                            rad = atan2(ball.vy, ball.vx)
                                            length = sqrt(ball.vx ** 2 + ball.vy ** 2)
                                            new_rad = -(rad - rad_C2P) + rad_C2P
                                            
                                            while new_rad < pi:
                                                new_rad += 2 * pi
                                            new_rad -= 2 * pi
                                            
                                            if new_rad >= 0 and new_rad < 0.15:
                                                new_rad = 0.15
                                            if new_rad >= -0.15 and new_rad < 0:
                                                new_rad = -0.15
                                            if new_rad >= pi - 0.15 and new_rad < pi:
                                                new_rad = pi - 0.15
                                            if new_rad >= -pi and new_rad < 0.15 - pi:
                                                new_rad = 0.15 - pi
                                            
                                            new_vec = {'x' : length * cos(new_rad), 'y' : length * sin(new_rad)}
                                            ball.vx = new_vec['x']
                                            ball.vy = new_vec['y']
                                        else:
                                            side = ''
                                            if point['x'] == 1:
                                                if point['y'] == 1:
                                                    if not passed['x1']:
                                                        side = 'down'
                                                    elif not passed['y1']:
                                                        side = 'right'
                                                else:#point['y'] == -1
                                                    if not passed['x1']:
                                                        side = 'up'
                                                    elif not passed['y-1']:
                                                        side = 'right'
                                            else: #point['x'] == -1
                                                if point['y'] == 1:
                                                    if not passed['x-1']:
                                                        side = 'down'
                                                    elif not passed['y1']:
                                                        side = 'left'
                                                else: #point['y'] == -1
                                                    if not passed['x-1']:
                                                        side = 'up'
                                                    elif not passed['y-1']:
                                                        side = 'left'
                                            
                                            if side == 'down':
                                                ball.vy = abs(ball.vy)
                                            elif side == 'left':
                                                ball.vx = -abs(ball.vx)
                                            elif side == 'up':
                                                ball.vy = -abs(ball.vy)
                                            elif side == 'right':
                                                ball.vx = abs(ball.vx)
                                    else:
                                        vec = {'x' : (ball.x - (x + 0.5) * 100 / 6) * 6, 'y' : (ball.y - (y + 0.5) * 100 / 9) * 9}
                                        rad = atan2(vec['y'], vec['x']) / pi
                                        if rad >= 1 / 4 and rad < 3 / 4: #Downside
                                            ball.vy = abs(ball.vy)
                                        elif rad >= 3 / 4 or rad < - 3 / 4: #Leftside
                                            ball.vx = -abs(ball.vx)
                                        elif rad >= - 3 / 4 and rad < - 1 / 4: #Upside
                                            ball.vy = -abs(ball.vy)
                                        elif rad >= - 1 / 4 or rad < 1 / 4: #Rightside
                                            ball.vx = abs(ball.vx)
                                    if block.lastHitId != ball.id or block.lastHitTime + dt * 15 < t: 
                                        block.num -= 1
                                        self.Data['Log'].append(str(x)+"/"+str(y)+":"+str(block.num))
                                        block.lastHitId = ball.id
                                        block.lastHitTime = t
                            elif block.type == 'ball':
                                vec = { 'x' : ball.x - 100 / 6 * (x + 0.5), 'y' : ball.y - 100 / 9 * (y + 0.5) }
                                length = sqrt(vec['x'] ** 2 + vec['y'] ** 2)
                                if length <= self.Game['Ball Radius'] * 2 and block.num > 0:
                                    block.num = 0
                                    self.Data['Log'].append("!"+str(x)+"/"+str(y)+":"+str(block.num))
                            #=========================================
            #Remove Block
            for y in range(self.Game['height']):
                for x in range(self.Game['width']):
                    block = self.Game['Map'][x][y]
                    if block:
                        if block.type == 'block':
                            if block.num <= 0:
                                self.Game['Map'][x][y] = None
                                continue
                        elif block.type == 'ball':
                            if block.num <= 0:
                                self.Game['Map'][x][y] = None
                                self.Game['Number of Balls'] += 1
                                continue
            #Remove Ball
            
            balls = self.Game['Shoot Balls']
            self.Game['Shoot Balls'] = []
            for ball in balls:
                if ball.y < 100 - self.Game['Ball Radius']:
                    self.Game['Shoot Balls'].append(ball)
                elif not self.Game['Ground Ball']:
                    self.Game['Ground Ball'] = True
                    self.Game['New Shoot Position'] = { 'x' : ball.x, 'y': self.Game['Shoot Position']['y'] }
                                
            #============================
    def close(self):
        if self.ui:
            pygame.quit()

if __name__ == '__main__':
    
    game = SBBGame()
    game.start()
    game.ui_start()

    t = 0
    while game.Run:
        game.loop(t)
        t += 1
    


        
