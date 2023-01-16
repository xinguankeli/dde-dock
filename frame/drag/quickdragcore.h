#ifndef QUICKDRAGCORE_H
#define QUICKDRAGCORE_H

#include <QMimeData>
#include <QDrag>
#include <QPixmap>

class PluginsItemInterface;
class QTimer;

class QuickPluginMimeData : public QMimeData
{
    Q_OBJECT

public:
    explicit QuickPluginMimeData(PluginsItemInterface *item, QDrag *drag);
    ~QuickPluginMimeData();
    PluginsItemInterface *pluginItemInterface() const;
    QDrag *drag() const;

private:
     PluginsItemInterface *m_item;
     QDrag *m_drag;
};

class QuickIconDrag : public QDrag
{
    Q_OBJECT

public:
    explicit QuickIconDrag(QObject *dragSource, const QPixmap &pixmap);
    ~QuickIconDrag();
    void updatePixmap(QPixmap pixmap);
    void useSourcePixmap();
    void setDragHotPot(QPoint point);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    QPoint currentPoint() const;

private Q_SLOTS:
    void onDragMove();

private:
    QWidget *m_imageWidget;
    QTimer *m_timer;
    QPixmap m_sourcePixmap;
    QPixmap m_pixmap;
    QPoint m_hotPoint;
    bool m_useSourcePixmap;
};

Q_DECLARE_INTERFACE(QuickIconDrag, "QuickIconDrag")

#endif // QUICKDRAGCORE_H
