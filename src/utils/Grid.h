#ifndef QI_GRID_H
#define QI_GRID_H

#include "Lines.h"
#include "CellID.h"
#include <QSharedPointer>
#include <QSize>
#include <QPoint>

namespace Qi
{

class QI_EXPORT Grid: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Grid)

public:
    Grid();
    Grid(const QSharedPointer<Lines>& rows, const QSharedPointer<Lines>& columns);
    virtual ~Grid();
    
    const Lines& rows() const;
    const Lines& columns() const;

    Lines& rows();
    Lines& columns();

    const QSharedPointer<Lines>& rowsPtr() const;
    const QSharedPointer<Lines>& columnsPtr() const;
    
    QSize dim() const;
    QSize visibleDim() const;
    
    QSize visibleSize() const;
    
    CellID toVisible(CellID absoluteCell) const;
    CellID toAbsolute(CellID visibleCell) const;
    
    CellID findVisCell(QPoint point) const;
    
signals:
    void gridChanged(const Grid&, ChangeReason);
    
private slots:
    void onLinesChanged(const Lines& lines, ChangeReason reason);
    
private:
    void connectLinesSignal();
    void disconnectLinesSignal();
    
    QSharedPointer<Lines> m_rows;
    QSharedPointer<Lines> m_columns;
};

} // end namespace Qi 

#endif // QI_GRID_H
