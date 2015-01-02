/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "qwt_plot_opengl_canvas.h"
#include "qwt_plot.h"
#include <qevent.h>
#include <qpainter.h>
#include <qdrawutil.h>
#include <qstyle.h>
#include <qstyleoption.h>

#define FIX_GL_TRANSLATION 0

#include <qopenglframebufferobject.h>
#include <qopenglpaintdevice.h>

#include "qwt_painter.h"

static QWidget *qwtBGWidget( QWidget *widget )
{
    QWidget *w = widget;

    for ( ; w->parentWidget() != NULL; w = w->parentWidget() )
    {
        if ( w->autoFillBackground() || 
            w->testAttribute( Qt::WA_StyledBackground ) )
        {
            return w;
        }
    }

    return w;
}

static void qwtUpdateContentsRect( QwtPlotOpenGLCanvas *canvas )
{
    const int fw = canvas->frameWidth();
    canvas->setContentsMargins( fw, fw, fw, fw );
}

class QwtPlotOpenGLCanvas::PrivateData
{
public:
    PrivateData():
        frameStyle( QFrame::Panel | QFrame::Sunken),
        lineWidth( 2 ),
        midLineWidth( 0 ),
        fbo( NULL )
    {
    }

    ~PrivateData()
    {
        delete fbo;
    }

    QwtPlotOpenGLCanvas::PaintAttributes paintAttributes;

    int frameStyle;
    int lineWidth;
    int midLineWidth;

    QOpenGLFramebufferObject* fbo;
};


/*! 
  \brief Constructor

  \param plot Parent plot widget
  \sa QwtPlot::setCanvas()
*/
QwtPlotOpenGLCanvas::QwtPlotOpenGLCanvas( QwtPlot *plot ):
    QOpenGLWidget( plot )
{
    QSurfaceFormat fmt = format();
    fmt.setSamples(16);
    setFormat( fmt );

    d_data = new PrivateData;
    init();
}

QwtPlotOpenGLCanvas::QwtPlotOpenGLCanvas( const QSurfaceFormat &format, QwtPlot *plot ):
    QOpenGLWidget( plot )
{
    setFormat( format );

    d_data = new PrivateData;
    init();
}

void QwtPlotOpenGLCanvas::init()
{
#ifndef QT_NO_CURSOR
    setCursor( Qt::CrossCursor );
#endif

    setAutoFillBackground( true );
    qwtUpdateContentsRect( this );

    setPaintAttribute( QwtPlotOpenGLCanvas::BackingStore, true );
}

//! Destructor
QwtPlotOpenGLCanvas::~QwtPlotOpenGLCanvas()
{
    delete d_data;
}

/*!
  \brief Changing the paint attributes

  \param attribute Paint attribute
  \param on On/Off

  \sa testPaintAttribute()
*/
void QwtPlotOpenGLCanvas::setPaintAttribute( PaintAttribute attribute, bool on )
{   
    if ( bool( d_data->paintAttributes & attribute ) == on )
        return;
    
    if ( on )
        d_data->paintAttributes |= attribute;
    else
        d_data->paintAttributes &= ~attribute;

    if ( attribute == BackingStore )
        invalidateBackingStore();
}

/*!
  Test whether a paint attribute is enabled

  \param attribute Paint attribute
  \return true, when attribute is enabled
  \sa setPaintAttribute()
*/  
bool QwtPlotOpenGLCanvas::testPaintAttribute( PaintAttribute attribute ) const
{   
    return d_data->paintAttributes & attribute;
}

/*!
  Set the frame style

  \param style The bitwise OR between a shape and a shadow. 
  
  \sa frameStyle(), QFrame::setFrameStyle(), 
      setFrameShadow(), setFrameShape()
 */
void QwtPlotOpenGLCanvas::setFrameStyle( int style )
{
    if ( style != d_data->frameStyle )
    {
        d_data->frameStyle = style;
        qwtUpdateContentsRect( this );

        update();
    }
}

/*!
  \return The bitwise OR between a frameShape() and a frameShadow()
  \sa setFrameStyle(), QFrame::frameStyle()
 */
int QwtPlotOpenGLCanvas::frameStyle() const
{
    return d_data->frameStyle;
}

/*!
  Set the frame shadow

  \param shadow Frame shadow
  \sa frameShadow(), setFrameShape(), QFrame::setFrameShadow()
 */
void QwtPlotOpenGLCanvas::setFrameShadow( Shadow shadow )
{
    setFrameStyle(( d_data->frameStyle & QFrame::Shape_Mask ) | shadow );
}

/*!
  \return Frame shadow
  \sa setFrameShadow(), QFrame::setFrameShadow()
 */
QwtPlotOpenGLCanvas::Shadow QwtPlotOpenGLCanvas::frameShadow() const
{
    return (Shadow) ( d_data->frameStyle & QFrame::Shadow_Mask );
}

/*!
  Set the frame shape

  \param shape Frame shape
  \sa frameShape(), setFrameShadow(), QFrame::frameShape()
 */
void QwtPlotOpenGLCanvas::setFrameShape( Shape shape )
{
    setFrameStyle( ( d_data->frameStyle & QFrame::Shadow_Mask ) | shape );
}

/*!
  \return Frame shape
  \sa setFrameShape(), QFrame::frameShape()
 */
QwtPlotOpenGLCanvas::Shape QwtPlotOpenGLCanvas::frameShape() const
{
    return (Shape) ( d_data->frameStyle & QFrame::Shape_Mask );
}

/*!
   Set the frame line width

   The default line width is 2 pixels.

   \param width Line width of the frame
   \sa lineWidth(), setMidLineWidth()
*/
void QwtPlotOpenGLCanvas::setLineWidth( int width )
{
    width = qMax( width, 0 );
    if ( width != d_data->lineWidth )
    {
        d_data->lineWidth = qMax( width, 0 );
        qwtUpdateContentsRect( this );
        update();
    }
}

/*!
  \return Line width of the frame
  \sa setLineWidth(), midLineWidth()
 */
int QwtPlotOpenGLCanvas::lineWidth() const
{
    return d_data->lineWidth;
}

/*!
   Set the frame mid line width

   The default midline width is 0 pixels.

   \param width Midline width of the frame
   \sa midLineWidth(), setLineWidth()
*/
void QwtPlotOpenGLCanvas::setMidLineWidth( int width )
{
    width = qMax( width, 0 );
    if ( width != d_data->midLineWidth )
    {
        d_data->midLineWidth = width;
        qwtUpdateContentsRect( this );
        update();
    }
}

/*!
  \return Midline width of the frame
  \sa setMidLineWidth(), lineWidth()
 */ 
int QwtPlotOpenGLCanvas::midLineWidth() const
{
    return d_data->midLineWidth;
}

/*!
  \return Frame width depending on the style, line width and midline width.
 */
int QwtPlotOpenGLCanvas::frameWidth() const
{
    return ( frameStyle() != NoFrame ) ? d_data->lineWidth : 0;
}

/*!
  Paint event

  \param event Paint event
  \sa QwtPlot::drawCanvas()
*/
void QwtPlotOpenGLCanvas::paintEvent( QPaintEvent *event )
{
    QOpenGLWidget::paintEvent( event );
}

/*!
  Qt event handler for QEvent::PolishRequest and QEvent::StyleChange
  \param event Qt Event
  \return See QGLWidget::event()
*/
bool QwtPlotOpenGLCanvas::event( QEvent *event )
{
    const bool ok = QOpenGLWidget::event( event );

    if ( event->type() == QEvent::PolishRequest ||
        event->type() == QEvent::StyleChange )
    {
        // assuming, that we always have a styled background
        // when we have a style sheet

        setAttribute( Qt::WA_StyledBackground,
            testAttribute( Qt::WA_StyleSheet ) );
    }

    return ok;
}

/*!
  Draw the plot items
  \param painter Painter

  \sa QwtPlot::drawCanvas()
*/  
void QwtPlotOpenGLCanvas::drawItems( QPainter *painter )
{
    painter->save();

    painter->setClipRect( contentsRect(), Qt::IntersectClip );

    QwtPlot *plot = qobject_cast< QwtPlot *>( parent() );
    if ( plot )
        plot->drawCanvas( painter );

    painter->restore();
}

/*!
  Draw the background of the canvas
  \param painter Painter
*/ 
void QwtPlotOpenGLCanvas::drawBackground( QPainter *painter )
{
    painter->save();

    QWidget *w = qwtBGWidget( this );

    const QPoint off = mapTo( w, QPoint() );
    painter->translate( -off );

    const QRect fillRect = rect().translated( off );

    if ( w->testAttribute( Qt::WA_StyledBackground ) )
    {
        painter->setClipRect( fillRect );

        QStyleOption opt;
        opt.initFrom( w );
        w->style()->drawPrimitive( QStyle::PE_Widget, &opt, painter, w);
    }
    else 
    {
#if 0
        if ( !autoFillBackground() )
#endif
        {
            painter->fillRect( fillRect,
                w->palette().brush( w->backgroundRole() ) );
        }
    }

    painter->restore();
}

/*!
  Draw the border of the canvas
  \param painter Painter
*/
void QwtPlotOpenGLCanvas::drawBorder( QPainter *painter )
{
    const int fw = frameWidth();
    if ( fw <= 0 )
        return;

    if ( frameShadow() == QwtPlotOpenGLCanvas::Plain )
    {
        qDrawPlainRect( painter, frameRect(), 
            palette().shadow().color(), lineWidth() );
    }
    else
    {
        if ( frameShape() == QwtPlotOpenGLCanvas::Box )
        {
            qDrawShadeRect( painter, frameRect(), palette(),
                frameShadow() == Sunken, lineWidth(), midLineWidth() );
        }
        else
        {
            qDrawShadePanel( painter, frameRect(), palette(), 
                frameShadow() == Sunken, lineWidth() );
        }
    }
}

void QwtPlotOpenGLCanvas::replot()
{
    invalidateBackingStore();

    if ( testPaintAttribute( QwtPlotOpenGLCanvas::ImmediatePaint ) )
        repaint( contentsRect() );
    else
        update( contentsRect() );
}

/*!
   \return Empty path
*/
QPainterPath QwtPlotOpenGLCanvas::borderPath( const QRect &rect ) const
{
    Q_UNUSED( rect );
    return QPainterPath();
}

QRect QwtPlotOpenGLCanvas::frameRect() const
{
    const int fw = frameWidth();
    return contentsRect().adjusted( -fw, -fw, fw, fw );
}

void QwtPlotOpenGLCanvas::invalidateBackingStore()
{
    delete d_data->fbo;
    d_data->fbo = NULL;
}

void QwtPlotOpenGLCanvas::initializeGL()
{
}

void QwtPlotOpenGLCanvas::paintGL()
{
    if ( testPaintAttribute( QwtPlotOpenGLCanvas::BackingStore ) )
    {
        if ( d_data->fbo == NULL || d_data->fbo->size() != size() )
        {
            invalidateBackingStore();

            const int numSamples = 16;

            QOpenGLFramebufferObjectFormat format;
            format.setSamples( numSamples );
            format.setAttachment( QOpenGLFramebufferObject::CombinedDepthStencil );

            QOpenGLFramebufferObject fbo( size(), format );

            QOpenGLPaintDevice pd( size() );

            QPainter painter( &pd );
            draw( &painter);
            painter.end();

            d_data->fbo = new QOpenGLFramebufferObject( size() );
            QOpenGLFramebufferObject::blitFramebuffer(d_data->fbo, &fbo );
        }

        glBindTexture(GL_TEXTURE_2D, d_data->fbo->texture());

        glEnable(GL_TEXTURE_2D);

        glBegin(GL_QUADS);

        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-1.0f,  1.0f);

        glEnd();
    }
    else
    {
        QPainter painter( this );
        draw( &painter );
    }
}

void QwtPlotOpenGLCanvas::resizeGL( int, int )
{
    invalidateBackingStore();
}

void QwtPlotOpenGLCanvas::draw( QPainter *painter )
{
#if FIX_GL_TRANSLATION
    if ( painter->paintEngine()->type() == QPaintEngine::OpenGL2 )
    {
        // work around a translation bug of QPaintEngine::OpenGL2
        painter->translate( 1, 1 );
    }
#endif

    drawBackground( painter );
    drawItems( painter );

    if ( !testAttribute( Qt::WA_StyledBackground ) )
    { 
        if ( frameWidth() > 0 )
            drawBorder( painter );
    }
}
