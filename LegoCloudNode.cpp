// Copyright 2012, 2013 Romain Testuz
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "LegoCloudNode.h"

#include <Dolphin/Core/Scenegraph/Visitor/Visitor.h>
#include <Dolphin/Core/Scenegraph/NodeTypes.h>
#include <Dolphin/Core/Scenegraph/Core/Material.h>
#include <Dolphin/Core/Scenegraph/Core/DrawModes.h>
#include <Dolphin/Core/DataStructures/Sphere.h>
#include <Dolphin/Core/DataStructures/AABB.h>

#include <Dolphin/Core/Utilities/dolphinstream.h>

#include <qmath.h>
#include <QGraphicsScene>
#include <iostream>
#include <fstream>

#include "LegoDimensions.h"
#include "LegoCloud.h"
#include "LegoGraph.h"

#define KNOB_RESOLUTION_DISPLAY 15
#define KNOB_RESOLUTION_OBJ_EXPORT 15


namespace Dolphin {
namespace scenegraph {


LegoCloudNode::LegoCloudNode(Node* _parent)
  : GeometryNode(_parent),
    legoCloud_(new LegoCloud()), renderLayerByLayer_(false), renderLayer_(0), knobList_(glGenLists(1)),
    renderBricks_(true), renderGraph_(false), colorRendering_(RealColor)
{

}

LegoCloudNode::~LegoCloudNode()
{
  delete legoCloud_;
  //dolphinOut() << "Node destroyed" << std::endl;
}

int LegoCloudNode::getType() const
{
  return LEGO_CLOUD_NODE;
}

std::string LegoCloudNode::getTypeName() const
{
  return "LegoCloudNode";
}

//CopyPaste
void LegoCloudNode::accept(Visitor & _visitor)
{
    if(_visitor.enter(this)) {
            for(int i=0; i<getNumberOfChildren(); ++i) {
                    getChild(i)->accept(_visitor);
            }
    }
    _visitor.leave(this);
}

void LegoCloudNode::recomputeBoundingSphere() {
  if(aabbIsDirty_)
    recomputeAABB();

  //boundingSphere_->setEmpty();
  boundingSphere_->setRadius(aabb_->getRadius());
  boundingSphere_->setCenter(aabb_->getCenter());

  boundingSphereIsDirty_  = false;
}

void LegoCloudNode::recomputeAABB()
{
  int minX = INT_MAX;
  int minLevel = 0;
  int minY = INT_MAX;

  int maxX = INT_MIN;
  int maxLevel = legoCloud_->getLevelNumber();
  int maxY = INT_MIN;

  for(int level=0; level < legoCloud_->getLevelNumber(); level++)
  {
    for(QList<LegoBrick>::const_iterator brick = legoCloud_->getBricks(level).begin(); brick != legoCloud_->getBricks(level).constEnd(); brick++)//QTL
    {
      if(brick->getPosX() < minX)
        minX = brick->getPosX();
      if(brick->getPosY() < minY)
        minY = brick->getPosY();

      if(brick->getPosX()+1 > maxX)
        maxX = brick->getPosX()+1;
      if(brick->getPosY()+1 > maxY)
        maxY = brick->getPosY()+1;
    }
  }

  aabb_->set(Point(minX*LEGO_KNOB_DISTANCE, minLevel*LEGO_HEIGHT, minY*LEGO_KNOB_DISTANCE),
             Point(maxX*LEGO_KNOB_DISTANCE, maxLevel*LEGO_HEIGHT, maxY*LEGO_KNOB_DISTANCE));

  aabbIsDirty_  = false;
}

void LegoCloudNode::resetSelection(const bool _notifyObservers)
{
}

void LegoCloudNode::fillDrawData(DrawModes* _drawmodes)
{
}

void LegoCloudNode::render(DrawModes* _drawmodes)
{
  /*float specularity_[4] = {0.7, 0.7, 0.7, 1.0};
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularity_);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32);*/
  glLineWidth(getDefaultMaterial()->getEdgeWidth());
  glPointSize(getDefaultMaterial()->getVertexRadius());

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  //glDisable(GL_LIGHTING);

  glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
  //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);                 // Set Line Antialiasing
  glEnable(GL_BLEND);                         // Enable Blending
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const LegoGraph& graph = legoCloud_->getLegoGraph();

  if(renderBricks_)
  {

    //glEnable(GL_LIGHTING);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);

    LegoGraph::vertex_iterator vertexIt, vertexItEnd;
    for (boost::tie(vertexIt, vertexItEnd) = boost::vertices(graph); vertexIt != vertexItEnd; ++vertexIt)
    {
      LegoBrick* brick = graph[*vertexIt].brick;
      if((!renderLayerByLayer_ && brick->isOuter())|| (renderLayerByLayer_ && brick->getLevel() == renderLayer_))
      {
        setColor(*vertexIt);
        drawLegoBrick(*brick);
        //drawNeighbourhood(*brick, legoCloud_->getNeighbours(brick));
      }
    }

    glColor3i(0,0,0);
    for (boost::tie(vertexIt, vertexItEnd) = boost::vertices(graph); vertexIt != vertexItEnd; ++vertexIt)
    {
      LegoBrick* brick = graph[*vertexIt].brick;
      if((!renderLayerByLayer_ && brick->isOuter())|| (renderLayerByLayer_ && brick->getLevel() == renderLayer_))
      {

        drawBrickOutline(*brick);
        //drawNeighbourhood(*brick, legoCloud_->getNeighbours(brick));
      }
    }
  }

  if(renderGraph_)
    drawLegoGraph(graph);
}




void LegoCloudNode::drawLegoBrick(const LegoBrick &brick) const
{
  Point p1;//Back corner down left
  p1[0] = brick.getPosX()*LEGO_KNOB_DISTANCE + LEGO_HORIZONTAL_TOLERANCE;
  p1[1] = brick.getLevel()*LEGO_HEIGHT;
  p1[2] = brick.getPosY()*LEGO_KNOB_DISTANCE + LEGO_HORIZONTAL_TOLERANCE;

  Point p2;//Front corner up right
  p2[0] = p1[0] + brick.getSizeX()*LEGO_KNOB_DISTANCE - LEGO_HORIZONTAL_TOLERANCE;
  p2[1] = p1[1] + LEGO_HEIGHT;
  p2[2] = p1[2] + brick.getSizeY()*LEGO_KNOB_DISTANCE - LEGO_HORIZONTAL_TOLERANCE;

  drawBox(p1, p2);

  drawKnobs(brick, p1);

}

void LegoCloudNode::drawBox(const Point &p1, const Point &p2) const
{
  glBegin(GL_QUADS);
  //Bottom
  glNormal3f(0.0, -1.0, 0.0);
  glVertex3f(p1[0], p1[1], p1[2]);
  glVertex3f(p2[0], p1[1], p1[2]);
  glVertex3f(p2[0], p1[1], p2[2]);
  glVertex3f(p1[0], p1[1], p2[2]);

  //Back
  glNormal3f(0.0, 0.0, -1.0);
  glVertex3f(p1[0], p1[1], p1[2]);
  glVertex3f(p1[0], p2[1], p1[2]);
  glVertex3f(p2[0], p2[1], p1[2]);
  glVertex3f(p2[0], p1[1], p1[2]);

  //Right
  glNormal3f(1.0, 0.0, 0.0);
  glVertex3f(p2[0], p1[1], p1[2]);
  glVertex3f(p2[0], p2[1], p1[2]);
  glVertex3f(p2[0], p2[1], p2[2]);
  glVertex3f(p2[0], p1[1], p2[2]);

  //Front
  glNormal3f(0.0, 0.0, 1.0);
  glVertex3f(p2[0], p2[1], p2[2]);
  glVertex3f(p1[0], p2[1], p2[2]);
  glVertex3f(p1[0], p1[1], p2[2]);
  glVertex3f(p2[0], p1[1], p2[2]);

  //Left
  glNormal3f(-1.0, 0.0, 0.0);
  glVertex3f(p1[0], p1[1], p1[2]);
  glVertex3f(p1[0], p1[1], p2[2]);
  glVertex3f(p1[0], p2[1], p2[2]);
  glVertex3f(p1[0], p2[1], p1[2]);

  //Top
  glNormal3f(0.0, 1.0, 0.0);
  glVertex3f(p2[0], p2[1], p2[2]);
  glVertex3f(p2[0], p2[1], p1[2]);
  glVertex3f(p1[0], p2[1], p1[2]);
  glVertex3f(p1[0], p2[1], p2[2]);
  glEnd();

  //____________
}

void LegoCloudNode::drawBrickOutline(const LegoBrick &brick) const
{
  Point p1;//Back corner down left
  p1[0] = brick.getPosX()*LEGO_KNOB_DISTANCE + LEGO_HORIZONTAL_TOLERANCE;
  p1[1] = brick.getLevel()*LEGO_HEIGHT;
  p1[2] = brick.getPosY()*LEGO_KNOB_DISTANCE + LEGO_HORIZONTAL_TOLERANCE;

  Point p2;//Front corner up right
  p2[0] = p1[0] + brick.getSizeX()*LEGO_KNOB_DISTANCE - LEGO_HORIZONTAL_TOLERANCE;
  p2[1] = p1[1] + LEGO_HEIGHT;
  p2[2] = p1[2] + brick.getSizeY()*LEGO_KNOB_DISTANCE - LEGO_HORIZONTAL_TOLERANCE;

  glBegin(GL_LINE_LOOP);
  //Right
  glVertex3f(p2[0], p1[1], p1[2]);
  glVertex3f(p2[0], p2[1], p1[2]);
  glVertex3f(p2[0], p2[1], p2[2]);
  glVertex3f(p2[0], p1[1], p2[2]);
  glEnd();

  glBegin(GL_LINE_LOOP);
  //Left
  glVertex3f(p1[0], p1[1], p1[2]);
  glVertex3f(p1[0], p1[1], p2[2]);
  glVertex3f(p1[0], p2[1], p2[2]);
  glVertex3f(p1[0], p2[1], p1[2]);
  glEnd();

  glBegin(GL_LINES);
  glVertex3f(p1[0], p1[1], p1[2]);
  glVertex3f(p2[0], p1[1], p1[2]);

  glVertex3f(p1[0], p1[1], p2[2]);
  glVertex3f(p2[0], p1[1], p2[2]);

  glVertex3f(p1[0], p2[1], p2[2]);
  glVertex3f(p2[0], p2[1], p2[2]);

  glVertex3f(p1[0], p2[1], p1[2]);
  glVertex3f(p2[0], p2[1], p1[2]);
  glEnd();


  //Draw knob outline:
  Point p;//Center of back left knob (top)
  p[0] = p1[0] + LEGO_KNOB_DISTANCE/2.0;
  p[1] = p1[1] + LEGO_HEIGHT + LEGO_KNOB_HEIGHT;
  p[2] = p1[2] + LEGO_KNOB_DISTANCE/2.0;

  for(int x = 0; x < brick.getSizeX(); ++x)
  {
    for(int y = 0; y < brick.getSizeY(); ++y)
    {

      //Draw loop on top
      glBegin(GL_LINE_LOOP);
      for(int i = 0; i <= KNOB_RESOLUTION_DISPLAY; ++i)
      {
        double angle = -i*(2*M_PI/double(KNOB_RESOLUTION_DISPLAY));
        glVertex3d(p[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS,
                   p[1],
                   p[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS);
      }
      glEnd();

      glBegin(GL_LINE_LOOP);
      for(int i = 0; i <= KNOB_RESOLUTION_DISPLAY; ++i)
      {
        double angle = -i*(2*M_PI/double(KNOB_RESOLUTION_DISPLAY));
        glVertex3d(p[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS,
                   p[1] - LEGO_KNOB_HEIGHT,
                   p[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS);
      }
      glEnd();
    }
  }

}

void LegoCloudNode::drawKnobs(const LegoBrick &brick, const Point &p1) const
{
  Point p;//Center of back left knob (top)
  p[0] = p1[0] + LEGO_KNOB_DISTANCE/2.0;
  p[1] = p1[1] + LEGO_HEIGHT + LEGO_KNOB_HEIGHT;
  p[2] = p1[2] + LEGO_KNOB_DISTANCE/2.0;

  //glColor3dv(brick.getColor());

  for(int x = 0; x < brick.getSizeX(); ++x)
  {
    for(int y = 0; y < brick.getSizeY(); ++y)
    {

      //Draw cylinder
      glBegin(GL_QUAD_STRIP);
      for(int i = 0; i <= KNOB_RESOLUTION_DISPLAY; ++i)
      {
        double angle = -i*(2*M_PI/double(KNOB_RESOLUTION_DISPLAY));
        glNormal3d(cos(angle), 0.0, sin(angle));
        glVertex3d(p[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS,
                   p[1],
                   p[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS);

        glVertex3d(p[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS,
                   p[1] - LEGO_KNOB_HEIGHT,
                   p[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS);
      }
      glEnd();

      //Draw disk on top
      glBegin(GL_TRIANGLE_FAN);
      glNormal3d(0.0, 1.0, 0.0);
      glVertex3d(p[0]+x*LEGO_KNOB_DISTANCE, p[1], p[2]+y*LEGO_KNOB_DISTANCE);
      for(int i = 0; i <= KNOB_RESOLUTION_DISPLAY; ++i)
      {
        double angle = -i*(2*M_PI/double(KNOB_RESOLUTION_DISPLAY));
        glVertex3d(p[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS,
                   p[1],
                   p[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS);
      }
      glEnd();
    }
  }
}

void LegoCloudNode::drawNeighbourhood(const LegoBrick &brick, const QSet<LegoBrick *> &neighbours) const
{

  const double DRAW_HEIGHT = 0.01;
  Point brickCenter;
  brickCenter[0] = brick.getPosX()*LEGO_KNOB_DISTANCE + (brick.getSizeX()*LEGO_KNOB_DISTANCE)/2.0;
  brickCenter[1] = brick.getLevel()*LEGO_HEIGHT + LEGO_KNOB_HEIGHT + DRAW_HEIGHT;
  brickCenter[2] = brick.getPosY()*LEGO_KNOB_DISTANCE + (brick.getSizeY()*LEGO_KNOB_DISTANCE)/2.0;

  //glLineWidth(2);
  glColor3dv(brick.getRandColor());

  Point neighbourCenter;

  glDisable(GL_LIGHTING);
  glBegin(GL_LINES);

  foreach(const LegoBrick* neighbour, neighbours)
  {
    neighbourCenter[0] = neighbour->getPosX()*LEGO_KNOB_DISTANCE + (neighbour->getSizeX()*LEGO_KNOB_DISTANCE)/2.0;
    neighbourCenter[1] = neighbour->getLevel()*LEGO_HEIGHT + LEGO_KNOB_HEIGHT + DRAW_HEIGHT;
    neighbourCenter[2] = neighbour->getPosY()*LEGO_KNOB_DISTANCE + (neighbour->getSizeY()*LEGO_KNOB_DISTANCE)/2.0;
    glVertex3dv(brickCenter.data());
    glVertex3dv(neighbourCenter.data());
  }

  glEnd();
  glEnable(GL_LIGHTING);
}

void LegoCloudNode::drawLegoGraph(const LegoGraph & graph) const
{
  //Draw vertices

  glDisable(GL_LIGHTING);
  glBegin(GL_POINTS);
  LegoGraph::vertex_iterator vertexIt, vertexItEnd;
  for (boost::tie(vertexIt, vertexItEnd) = boost::vertices(graph); vertexIt != vertexItEnd; ++vertexIt)
  {
    const LegoBrick* brick = graph[*vertexIt].brick;
    if(!renderLayerByLayer_ || (renderLayerByLayer_ && (brick->getLevel() == renderLayer_ || brick->getLevel() == renderLayer_+1)))
    {
      Point brickCenter;
      brickCenter[0] = brick->getPosX()*LEGO_KNOB_DISTANCE + (brick->getSizeX()*LEGO_KNOB_DISTANCE)/2.0;
      brickCenter[1] = brick->getLevel()*LEGO_HEIGHT + LEGO_HEIGHT;
      brickCenter[2] = brick->getPosY()*LEGO_KNOB_DISTANCE + (brick->getSizeY()*LEGO_KNOB_DISTANCE)/2.0;

      //glColor3d(1,0,0);
      setColor(*vertexIt);
      glVertex3dv(brickCenter.data());
    }
  }
  glEnd();

  //Draw edges
  LegoGraph::edge_iterator edgeIt, edgeEnd;
  //glColor3d(0,0,1);

  glBegin(GL_LINES);
  for (boost::tie(edgeIt, edgeEnd) = boost::edges(graph); edgeIt != edgeEnd; ++edgeIt)
  {
    const LegoBrick* source = graph[boost::source(*edgeIt, graph)].brick;
    const LegoBrick* target = graph[boost::target(*edgeIt, graph)].brick;
    if(!renderLayerByLayer_ || (renderLayerByLayer_ &&
                                (source->getLevel() == renderLayer_ || source->getLevel() == renderLayer_+1) &&
                                (target->getLevel() == renderLayer_ || target->getLevel() == renderLayer_+1)))
    {
      Point sourceCenter;
      sourceCenter[0] = source->getPosX()*LEGO_KNOB_DISTANCE + (source->getSizeX()*LEGO_KNOB_DISTANCE)/2.0;
      sourceCenter[1] = source->getLevel()*LEGO_HEIGHT + LEGO_HEIGHT;
      sourceCenter[2] = source->getPosY()*LEGO_KNOB_DISTANCE + (source->getSizeY()*LEGO_KNOB_DISTANCE)/2.0;

      Point targetCenter;
      targetCenter[0] = target->getPosX()*LEGO_KNOB_DISTANCE + (target->getSizeX()*LEGO_KNOB_DISTANCE)/2.0;
      targetCenter[1] = target->getLevel()*LEGO_HEIGHT + LEGO_HEIGHT;
      targetCenter[2] = target->getPosY()*LEGO_KNOB_DISTANCE + (target->getSizeY()*LEGO_KNOB_DISTANCE)/2.0;

      glColor3d(0,0,1);

      glVertex3dv(sourceCenter.data());
      glVertex3dv(targetCenter.data());
    }

  }
  glEnd();
}

void LegoCloudNode::setColor(const LegoGraph::vertex_descriptor& vertex) const
{
  const LegoGraph& graph = legoCloud_->getLegoGraph();

  switch(colorRendering_)
  {
    case RealColor:
      glColor3dv(legoCloud_->getLegalColor()[graph[vertex].brick->getColorId()]);
      break;

    case Random:
      //glColor3d(boost::out_degree(vertex, graph)/10.0, 0.0, 0.0);

      //glColor3dv(graph[vertex].brick->getRandColor());
      glColor3dv(legoCloud_->getLegalColor()[graph[vertex].brick->getHash() % legoCloud_->getLegalColor().size()]);
      /*if(graph[vertex].brick->isOuter())
        glColor3dv(graph[vertex].brick->getRandColor());
      else
        glColor3dv(graph[vertex].brick->getRandColor() + Color3(0.5, 0,0));*/
      break;

    case ConnectedComp:
      {
        int red = 31 + graph[vertex].connected_comp;
        int green = 31*red + graph[vertex].connected_comp;
        int blue = 31*green + graph[vertex].connected_comp;
        int mod = 50;

        glColor3d((red%mod)/double(mod),
                  (green%mod)/double(mod),
                  (blue%mod)/double(mod));
      }
      break;

    case BiconnectedComp:
      if(graph[vertex].badArticulationPoint)
      {
        glColor3d(1.0,0.0,0.0);
      }
      else
      {
        glColor3dv(graph[vertex].brick->getRandColor());
      }
      break;

    default:
      glColor3d(0.0,0.0,0.0);
  }
}

void LegoCloudNode::drawInstructions(QGraphicsScene *scene, bool hintLayerBelow)
{
  const int BRICK_PIXEL_SIZE = 20;

  scene->clear();
  scene->setSceneRect(0, 0, legoCloud_->getWidth()*BRICK_PIXEL_SIZE, legoCloud_->getDepth()*BRICK_PIXEL_SIZE);

  for(QList<LegoBrick>::const_iterator brick = legoCloud_->getBricks(renderLayer_).begin(); brick != legoCloud_->getBricks(renderLayer_).constEnd(); brick++)//QTL
  {
    Color3 color = legoCloud_->getLegalColor()[brick->getColorId()];
    scene->addRect(brick->getPosX()*BRICK_PIXEL_SIZE, brick->getPosY()*BRICK_PIXEL_SIZE, brick->getSizeX()*BRICK_PIXEL_SIZE, brick->getSizeY()*BRICK_PIXEL_SIZE, QPen(),
                   QBrush(QColor(color[0]*255, color[1]*255, color[2]*255), Qt::SolidPattern));

    for(int x = 0; x < brick->getSizeX(); ++x)
    {
      for(int y = 0; y < brick->getSizeY(); ++y)
      {
        double knobRadius = LEGO_KNOB_RADIUS / LEGO_KNOB_DISTANCE;
        scene->addEllipse((brick->getPosX() + x + (0.5 - knobRadius))*BRICK_PIXEL_SIZE, (brick->getPosY() + y + (0.5 - knobRadius))*BRICK_PIXEL_SIZE, 2.0*knobRadius*BRICK_PIXEL_SIZE, 2.0*knobRadius*BRICK_PIXEL_SIZE);
      }
    }
  }

  //Draw the underneath layer (if there is one)
  if(renderLayer_ >= 1 && hintLayerBelow)
  {
    //foreach(const LegoBrick& brick, legoCloud_->getBricks(renderLayer_-1))
    for(QList<LegoBrick>::const_iterator brick = legoCloud_->getBricks(renderLayer_-1).begin(); brick != legoCloud_->getBricks(renderLayer_-1).constEnd(); brick++)
    {
      QColor color(0, 0, 0, 200);
      //scene->addRect(brick.getPosX(), brick.getPosY(), brick.getSizeX(), brick.getSizeY(), QPen(Qt::NoPen), QBrush(color, Qt::SolidPattern));
      scene->addRect(brick->getPosX()*BRICK_PIXEL_SIZE, brick->getPosY()*BRICK_PIXEL_SIZE, brick->getSizeX()*BRICK_PIXEL_SIZE,
                     brick->getSizeY()*BRICK_PIXEL_SIZE, QPen(Qt::NoPen), QBrush(color, Qt::Dense5Pattern));
    }
  }
}

void LegoCloudNode::exportToObj(QString filename)
{
  //Create the file
  ofstream objFile (filename.toStdString().c_str());
  if (!objFile.is_open())
  {
    dolphinErr() << "LegoCloudNode: unable to create or open the file: " << filename.toStdString().c_str() << std::endl;
  }

  const float LEGO_VERTICAL_TOLERANCE = 0.0001;
  int brickIndex = 0;
  int vertexIndex = 1;
  //const QList<LegoBrick*>& outterBricks = legoCloud_->getOuterBricks();

  for(int level = 0; level < legoCloud_->getLevelNumber(); level++)
  {
    for(QList<LegoBrick>::const_iterator brickIt = legoCloud_->getBricks(level).begin(); brickIt != legoCloud_->getBricks(level).constEnd(); brickIt++)
    {
      const LegoBrick* brick = &(*brickIt);

      Point p1;//Back corner down left
      p1[0] = brick->getPosX()*LEGO_KNOB_DISTANCE + LEGO_HORIZONTAL_TOLERANCE;
      p1[1] = brick->getLevel()*LEGO_HEIGHT + LEGO_VERTICAL_TOLERANCE;
      p1[2] = brick->getPosY()*LEGO_KNOB_DISTANCE + LEGO_HORIZONTAL_TOLERANCE;
      //p1 *= 10.0;

      Point p2;//Front corner up right
      p2[0] = p1[0] + brick->getSizeX()*LEGO_KNOB_DISTANCE - LEGO_HORIZONTAL_TOLERANCE;
      p2[1] = p1[1] + LEGO_HEIGHT - LEGO_VERTICAL_TOLERANCE;
      p2[2] = p1[2] + brick->getSizeY()*LEGO_KNOB_DISTANCE - LEGO_HORIZONTAL_TOLERANCE;
      //p2 *= 10.0;

      Point knobCenter;//Center of back left knob (top)
      knobCenter[0] = p1[0] + LEGO_KNOB_DISTANCE/2.0;
      knobCenter[1] = p1[1] + LEGO_HEIGHT + LEGO_KNOB_HEIGHT;
      knobCenter[2] = p1[2] + LEGO_KNOB_DISTANCE/2.0;


      objFile << "g default" << std::endl;
      //Write 8 vertices of box
      objFile << "v " << p1[0] << " " << p1[1] << " " << p1[2] << std::endl;
      objFile << "v " << p1[0] << " " << p1[1] << " " << p2[2] << std::endl;
      objFile << "v " << p1[0] << " " << p2[1] << " " << p1[2] << std::endl;
      objFile << "v " << p1[0] << " " << p2[1] << " " << p2[2] << std::endl;
      objFile << "v " << p2[0] << " " << p1[1] << " " << p1[2] << std::endl;
      objFile << "v " << p2[0] << " " << p1[1] << " " << p2[2] << std::endl;
      objFile << "v " << p2[0] << " " << p2[1] << " " << p1[2] << std::endl;
      objFile << "v " << p2[0] << " " << p2[1] << " " << p2[2] << std::endl << std::endl;

      //Write knobs vertices
      for(int x = 0; x < brick->getSizeX(); ++x)
      {
        for(int y = 0; y < brick->getSizeY(); ++y)
        {

          //Cylinder
          /*objFile << "v "
                  << knobCenter[0] + x*LEGO_KNOB_DISTANCE << " "
                  << knobCenter[1] << " "
                  << knobCenter[2] + y*LEGO_KNOB_DISTANCE << std::endl;*/

          for(int i = 0; i < KNOB_RESOLUTION_OBJ_EXPORT; ++i)
          {
            double angle = -i*(2*M_PI/double(KNOB_RESOLUTION_OBJ_EXPORT));
            objFile << "v "
                    << knobCenter[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS << " "
                    << knobCenter[1] << " "
                    << knobCenter[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS << std::endl;

            objFile << "v "
                    << knobCenter[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS << " "
                    << knobCenter[1] - LEGO_KNOB_HEIGHT << " "
                    << knobCenter[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS << std::endl;

          }

          /*
          //Draw disk on top
          glBegin(GL_TRIANGLE_FAN);
          glNormal3d(0.0, 1.0, 0.0);
          glVertex3d(knobCenter[0]+x*LEGO_KNOB_DISTANCE, knobCenter[1], knobCenter[2]+y*LEGO_KNOB_DISTANCE);
          for(int i = 0; i <= KNOB_RESOLUTION_OBJ_EXPORT; ++i)
          {
            double angle = -i*(2*M_PI/double(KNOB_RESOLUTION_OBJ_EXPORT));
            glVertex3d(knobCenter[0] + x*LEGO_KNOB_DISTANCE + cos(angle)*LEGO_KNOB_RADIUS,
                       knobCenter[1],
                       knobCenter[2] + y*LEGO_KNOB_DISTANCE + sin(angle)*LEGO_KNOB_RADIUS);
          }
          glEnd();*/
        }
      }

      //objFile << std::endl << "g boxes brick" << brickIndex << std::endl;
      objFile << std::endl << "g brick" << brickIndex << std::endl;
      objFile << "s off" << std::endl;

      //Write box faces
      objFile << "f " << vertexIndex+0 << " " << vertexIndex+1 << " " << vertexIndex+3 << " " << vertexIndex+2 << std::endl;//left
      objFile << "f " << vertexIndex+4 << " " << vertexIndex+6 << " " << vertexIndex+7 << " " << vertexIndex+5 << std::endl;//right
      objFile << "f " << vertexIndex+0 << " " << vertexIndex+4 << " " << vertexIndex+5 << " " << vertexIndex+1 << std::endl;//bottom
      objFile << "f " << vertexIndex+2 << " " << vertexIndex+3 << " " << vertexIndex+7 << " " << vertexIndex+6 << std::endl;//top
      objFile << "f " << vertexIndex+0 << " " << vertexIndex+2 << " " << vertexIndex+6 << " " << vertexIndex+4 << std::endl;//back
      objFile << "f " << vertexIndex+1 << " " << vertexIndex+5 << " " << vertexIndex+7 << " " << vertexIndex+3 << std::endl;//front
      /*objFile << "f " << vertexIndex+0 << " " << vertexIndex+1 << " " << vertexIndex+3 << std::endl;//left
      objFile << "f " << vertexIndex+0 << " " << vertexIndex+3 << " " << vertexIndex+2 << std::endl;//left

      objFile << "f " << vertexIndex+4 << " " << vertexIndex+6 << " " << vertexIndex+7 << std::endl;//right
      objFile << "f " << vertexIndex+4 << " " << vertexIndex+7 << " " << vertexIndex+5 << std::endl;//right

      objFile << "f " << vertexIndex+0 << " " << vertexIndex+4 << " " << vertexIndex+5 << std::endl;//bottom
      objFile << "f " << vertexIndex+0 << " " << vertexIndex+5 << " " << vertexIndex+1 << std::endl;//bottom

      objFile << "f " << vertexIndex+2 << " " << vertexIndex+3 << " " << vertexIndex+7 << std::endl;//top
      objFile << "f " << vertexIndex+2 << " " << vertexIndex+7 << " " << vertexIndex+6 << std::endl;//top

      objFile << "f " << vertexIndex+0 << " " << vertexIndex+2 << " " << vertexIndex+6 << std::endl;//back
      objFile << "f " << vertexIndex+0 << " " << vertexIndex+6 << " " << vertexIndex+4 << std::endl;//back

      objFile << "f " << vertexIndex+1 << " " << vertexIndex+5 << " " << vertexIndex+7 << std::endl;//front
      objFile << "f " << vertexIndex+1 << " " << vertexIndex+7 << " " << vertexIndex+3 << std::endl;//front*/

      //objFile << std::endl << "g knobs brick" << brickIndex << std::endl;

      //Write top caps face
      for(int x = 0; x < brick->getSizeX(); ++x)
      {
        for(int y = 0; y < brick->getSizeY(); ++y)
        {
          int knobIndex = vertexIndex+8 + (x*brick->getSizeY() + y) * (2*KNOB_RESOLUTION_OBJ_EXPORT);
          objFile << "f";
          for(int i = 0; i < KNOB_RESOLUTION_OBJ_EXPORT; ++i)//The degree of the face equals KNOB_RESOLUTION_OBJ_EXPORT
          {
            objFile << " " << knobIndex + 2*i;
          }
          objFile << std::endl;
        }
      }

      //Write cylinder faces
      objFile << "s 1" << std::endl;
      for(int x = 0; x < brick->getSizeX(); ++x)
      {
        for(int y = 0; y < brick->getSizeY(); ++y)
        {
          int knobIndex = vertexIndex+8 + (x*brick->getSizeY() + y) * (2*KNOB_RESOLUTION_OBJ_EXPORT);

          for(int i = 0; i < KNOB_RESOLUTION_OBJ_EXPORT; ++i)
          {
            //Check if it is not the last face
            if(i != KNOB_RESOLUTION_OBJ_EXPORT-1)
              objFile << "f " << knobIndex + 2*i+0 << " " << knobIndex + 2*i+1 << " " << knobIndex + 2*i+3 << " " << knobIndex + 2*i+2 << std::endl;
            else//The last face should connect to the first vertices (0 and 1)
              objFile << "f " << knobIndex + 2*i+0 << " " << knobIndex + 2*i+1 << " " << knobIndex + 1 << " " << knobIndex + 0 << std::endl;

          }
        }
      }

      brickIndex++;
      vertexIndex += 8 + brick->getSizeX()*brick->getSizeY()*(2*KNOB_RESOLUTION_OBJ_EXPORT);
    }
  }

  objFile.close();
}

}}//namespaces
