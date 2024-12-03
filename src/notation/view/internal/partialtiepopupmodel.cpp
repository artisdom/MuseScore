#include "partialtiepopupmodel.h"
#include "dom/partialtie.h"
#include "dom/tie.h"
#include "types/typesconv.h"
#include "dom/undo.h"

using namespace mu::notation;
using namespace mu::engraving;
using namespace muse::uicomponents;
using namespace muse::ui;

PartialTiePopupModel::PartialTiePopupModel(QObject* parent)
    : AbstractElementPopupModel(PopupModelType::TYPE_PARTIAL_TIE, parent)
{
}

bool PartialTiePopupModel::tieDirection() const
{
    if (!m_item) {
        return false;
    }
    const Tie* tieItem = tie();
    return tieItem->up();
}

bool PartialTiePopupModel::canOpen() const
{
    Tie* tieItem = tie();
    if (!tieItem || !tieItem->tieEndPoints()) {
        return false;
    }

    if (tieItem->tieEndPoints()->size() < 2) {
        return false;
    }

    return tieItem->isPartialTie() ? toPartialTie(tieItem)->isOutgoing() : true;
}

QPointF PartialTiePopupModel::dialogPosition() const
{
    const Tie* tieItem = tie();
    const Note* startNote = tieItem ? tieItem->startNote() : nullptr;
    const TieSegment* seg = toTieSegment(m_item);
    if (!seg || !startNote) {
        return QPointF();
    }
    const RectF segRect = seg->canvasBoundingRect();
    const double x = startNote->canvasBoundingRect().x();
    const int up = tieItem->up() ? -1 : 1;
    const double y = (tieItem->up() ? segRect.top() + segRect.height() * 2
                      / 3 : segRect.bottom() - segRect.height() / 3) + tieItem->spatium() * up;

    return fromLogical(PointF(x, y)).toQPointF();
}

QVariantList PartialTiePopupModel::items() const
{
    QVariantList items;

    for (MenuItem* item: m_items) {
        items << QVariant::fromValue(item);
    }

    return items;
}

void PartialTiePopupModel::init()
{
    AbstractElementPopupModel::init();

    connect(this, &AbstractElementPopupModel::dataChanged, [this]() {
        load();
    });

    load();
}

void PartialTiePopupModel::toggleItemChecked(QString& id)
{
    Tie* tieItem = tie();
    if (!tieItem || !tieItem->tieEndPoints()) {
        return;
    }

    for (MenuItem* item : m_items) {
        if (item->id() != id) {
            continue;
        }
        UiActionState state = item->state();
        state.checked = !state.checked;
        item->setState(state);
        break;
    }

    TieEndPointList* ends = tieItem->tieEndPoints();
    ends->toggleEndPoint(id);
    Tie* newTie = ends->startTie();

    // Update popup item if it has changed
    if (newTie && newTie != tieItem) {
        m_item = newTie->segmentsEmpty() ? nullptr : newTie->frontSegment();

        interaction()->endEditGrip();
        interaction()->endEditElement();
        interaction()->startEditGrip(m_item, Grip::DRAG);
    }

    updateNotation();
    emit itemsChanged();
}

void PartialTiePopupModel::load()
{
    Tie* tieItem = tie();
    if (!tieItem) {
        return;
    }

    tieItem->collectPossibleEndPoints();

    m_items = makeMenuItems();

    // load items
    emit tieDirectionChanged(tieDirection());
    emit itemsChanged();
}

MenuItemList PartialTiePopupModel::makeMenuItems()
{
    Tie* tieItem = tie();
    if (!tieItem || !tieItem->tieEndPoints()) {
        return MenuItemList{};
    }

    MenuItemList itemList;

    for (const TieEndPoint& endPoint : *tieItem->tieEndPoints()) {
        itemList << makeMenuItem(endPoint);
    }

    return itemList;
}

muse::uicomponents::MenuItem* PartialTiePopupModel::makeMenuItem(const engraving::TieEndPoint& endPoint)
{
    MenuItem* item = new MenuItem(this);
    item->setId(endPoint.id());
    TranslatableString title = TranslatableString("notation", endPoint.menuTitle());
    item->setTitle(title);

    UiAction action;
    action.title = title;
    action.checkable = Checkable::Yes;
    item->setAction(action);

    UiActionState state;
    state.enabled = true;
    state.checked = endPoint.active();
    item->setState(state);

    return item;
}

Tie* PartialTiePopupModel::tie() const
{
    const TieSegment* tieSeg = m_item && m_item->isTieSegment() ? toTieSegment(m_item) : nullptr;

    return tieSeg ? tieSeg->tie() : nullptr;
}

void mu::notation::PartialTiePopupModel::onClosed()
{
    Tie* tieItem = tie();
    if (!tieItem) {
        return;
    }

    if (tieItem->allEndPointsInactive()) {
        Score* score = tieItem->score();
        beginCommand(TranslatableString("engraving", "Remove partial tie"));
        score->undoRemoveElement(tieItem);
        endCommand();

        // Combine this with the last undoable action (which will be to remove a tie) so the user cannot undo to get a translucent tie
        undoStack()->mergeCommands(undoStack()->currentStateIndex() - 2);
    }
}
