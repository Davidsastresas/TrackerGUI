#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRunnable>
#include <gst/gst.h>

class SetPlaying : public QRunnable
{
public:
  SetPlaying(GstElement *);
  ~SetPlaying();

  void run ();

private:
  GstElement * pipeline_;
};

SetPlaying::SetPlaying (GstElement * pipeline)
{
  this->pipeline_ = pipeline ? static_cast<GstElement *> (gst_object_ref (pipeline)) : NULL;
}

SetPlaying::~SetPlaying ()
{
  if (this->pipeline_)
    gst_object_unref (this->pipeline_);
}

void
SetPlaying::run ()
{
  if (this->pipeline_)
    gst_element_set_state (this->pipeline_, GST_STATE_PLAYING);
    qCritical() << "state set to playing";
}

int main(int argc, char *argv[])
{
  int ret;

  gst_init (&argc, &argv);

  {
    QGuiApplication app(argc, argv);

    GstElement *pipeline = gst_pipeline_new (NULL);
    GstElement *src = gst_element_factory_make ("udpsrc", "udp");
    GstElement *demux = gst_element_factory_make("rtph264depay", "rtp-h264-depacketizer");
    GstElement *parser = gst_element_factory_make("h264parse", "h264-parser");
    GstElement *decoder = gst_element_factory_make("avdec_h264", "h264-decoder");
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    GstElement *glupload = gst_element_factory_make ("glupload", NULL);
    GstElement *glcolorconvert = gst_element_factory_make ("glcolorconvert", NULL);
    GstCaps *caps = NULL;

    if ((caps = gst_caps_from_string("application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264")) == NULL) {
        qCritical() << "VideoReceiver::start() failed. Error with gst_caps_from_string()";
    }

    g_object_set(G_OBJECT(src), "caps", caps, NULL);

    /* the plugin must be loaded before loading the qml file to register the
     * GstGLVideoItem qml item */
    GstElement *sink = gst_element_factory_make ("qmlglsink", NULL);

    g_object_set(G_OBJECT(src), "port", 5004, NULL);


    g_assert (src && glupload && sink);

    gst_bin_add_many (GST_BIN (pipeline), src, demux, parser, decoder, videoconvert, glupload, glcolorconvert, sink, NULL);
    if(!gst_element_link_many (src, demux, parser, decoder, videoconvert, glupload, glcolorconvert, sink, NULL)){
        qCritical() << "failed to link elements";
    }

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    QQuickItem *videoItem;
    QQuickWindow *rootObject;

    /* find and set the videoItem on the sink */
    rootObject = static_cast<QQuickWindow *> (engine.rootObjects().first());
    videoItem = rootObject->findChild<QQuickItem *> ("videoItem");
    g_assert (videoItem);
    g_object_set(sink, "widget", videoItem, NULL);

    rootObject->scheduleRenderJob (new SetPlaying (pipeline),
        QQuickWindow::BeforeSynchronizingStage);

    ret = app.exec();

    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
  }

  gst_deinit ();

  return ret;
}
