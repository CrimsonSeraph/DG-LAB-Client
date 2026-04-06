#include "FormulaBuilderDialog.h"
#include "ValueModeDelegate.h"

#include <QEvent>
#include <QMouseEvent>

ValueModeDelegate::ValueModeDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QWidget* ValueModeDelegate::createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const {
    return nullptr; // 禁用默认文本编辑
}

bool ValueModeDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
    const QStyleOptionViewItem&, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // 取出当前显示文本并还原为原始格式
            QString display = index.data(Qt::DisplayRole).toString();
            QString raw = display;
            raw.replace("{   }", "{}");

            FormulaBuilderDialog dlg(raw, qobject_cast<QWidget*>(parent()));
            if (dlg.exec() == QDialog::Accepted) {
                QString newRaw = dlg.get_formula();
                QString newDisplay = newRaw;
                newDisplay.replace("{}", "{   }");
                model->setData(index, newDisplay, Qt::EditRole);
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, QStyleOptionViewItem(), index);
}
