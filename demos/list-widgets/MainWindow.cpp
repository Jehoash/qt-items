#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "core/Layout.h"
#include "core/ext/Ranges.h"
#include "core/ext/ModelStore.h"
#include "items/misc/ViewItemBorder.h"
#include "items/misc/ViewAlternateBackground.h"
#include "items/misc/ControllerMouseLinesResizer.h"
#include "items/visible/Visible.h"
#include "items/sorting/Sorting.h"
#include "items/filter/FilterText.h"
#include "items/checkbox/Check.h"
#include "items/radiobutton/Radio.h"
#include "items/button/Button.h"
#include "items/numeric/Numeric.h"
#include "items/selection/Selection.h"
#include "items/image/StyleStandardPixmap.h"
#include "items/image/Image.h"
#include "items/link/Link.h"
#include "items/progressbar/Progress.h"
#include "items/enum/Enum.h"

#include "misc/GridColumnsResizer.h"
#include "cache/space/CacheSpaceGrid.h"
#include "cache/CacheItem.h"
#include "utils/CallLater.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QPauseAnimation>
#include <QVariantAnimation>

using namespace Qi;
/*
class AnimationCallback: public QAbstractAnimation
{
public:
    AnimationCallback(QObject* parent, const std::function<void(qreal)>& callback)
        : QAbstractAnimation(parent),
          m_duration(-1),
          m_callback(callback)
    {
        m_callback(0);
    }

    ~AnimationCallback()
    {

    }

    int duration() const override { return m_duration; }
    void setDuration(int duration) { m_duration = duration; }

    QEasingCurve easingCurve() const { return m_easingCurve; }
    void setEasingCurve(const QEasingCurve & easing) { m_easingCurve = easing; }

protected:
    void updateCurrentTime(int currentTime) override
    {
        qreal progress = m_easingCurve.valueForProgress((qreal)currentTime / (qreal)totalDuration());
        m_callback(progress);
    }

private:
    int m_duration;
    QEasingCurve m_easingCurve;
    std::function<void(qreal)> m_callback;
};
*/
class CacheAnimationAbstract: public QAbstractAnimation
{
public:
    CacheAnimationAbstract(QWidget* widget, CacheSpace* cacheSpace)
        : QAbstractAnimation(widget),
          m_animation(nullptr),
          m_widget(widget),
          m_cacheSpace(cacheSpace)
    {
    }

    virtual ~CacheAnimationAbstract()
    {
        //disconnect(m_cacheSpace, &CacheSpace::cacheChanged, this, &CacheAnimationAbstract::onCacheChanged);
    }

    int duration () const override { return m_animation ? m_animation->duration() : 0; }

    QEasingCurve easingCurve() const { return m_easingCurve; }
    void setEasingCurve(const QEasingCurve & easing) { m_easingCurve = easing; }

    void launch()
    {
        connect(m_cacheSpace, &CacheSpace::preDraw, this, &CacheAnimationAbstract::onPreDraw);
        m_widget->repaint();
    }

    virtual QAbstractAnimation* createAnimation(QWidget* widget, CacheSpace* cacheSpace) = 0;

protected:
    void updateCurrentTime(int /*currentTime*/) override
    {
        m_widget->repaint();
    }

    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State /*oldState*/) override
    {
        if (newState == Stopped)
        {
            m_cacheSpace->clear();
        }
    }

private:
    void onPreDraw()
    {
        disconnect(m_cacheSpace, &CacheSpace::preDraw, this, &CacheAnimationAbstract::onPreDraw);
        connect(m_cacheSpace, &CacheSpace::cacheChanged, this, &CacheAnimationAbstract::onCacheChanged);

        m_animation = createAnimation(m_widget, m_cacheSpace);

        callLater(this, [this](){
            m_animation->setDirection(direction());
            m_animation->setCurrentTime(currentTime());
            m_animation->start(DeleteWhenStopped);
            start(DeleteWhenStopped);
        });
    }

    void onCacheChanged(const CacheSpace* cache, ChangeReason reason)
    {
        Q_UNUSED(cache);
        Q_UNUSED(reason);
        Q_ASSERT(cache == m_cacheSpace);
        if (!(reason&ChangeReasonSpaceItemsContent))
            stop();
    }

    QEasingCurve m_easingCurve;
    QAbstractAnimation* m_animation;

    QWidget* m_widget;
    CacheSpace* m_cacheSpace;
};

class CacheAnimationShiftRight : public CacheAnimationAbstract
{
public:
    CacheAnimationShiftRight(QWidget* widget, CacheSpace* cacheSpace)
        : CacheAnimationAbstract(widget, cacheSpace)
    {}

    QAbstractAnimation* createAnimation(QWidget* /*widget*/, CacheSpace* cacheSpace) override
    {
        auto animation = new QParallelAnimationGroup(this);
        // init
        cacheSpace->forEachCacheView([this, animation, cacheSpace](const CacheSpace::IterateInfo& info)->bool {
            auto ss = new QSequentialAnimationGroup(animation);

            auto va = new QVariantAnimation(ss);
            va->setDuration(1000);
            va->setEasingCurve(easingCurve());
            QRect startRect = info.cacheView->rect();
            startRect.moveTo(cacheSpace->window().left() - startRect.width(), startRect.top());
            va->setStartValue(startRect);
            va->setEndValue(info.cacheView->rect());
            CacheView* cv = info.cacheView;
            connect(va, &QVariantAnimation::valueChanged, [cv](const QVariant &value){
                cv->rRect() = value.toRect();
            });
            ss->addPause(info.cacheItemIndex*100);
            ss->addAnimation(va);

            animation->addAnimation(ss);

            if (direction() == QAbstractAnimation::Forward)
            {
                info.cacheView->rRect() = startRect;
            }

            return true;
        });

        return animation;
    }
};

class CacheAnimationShiftLeft : public CacheAnimationAbstract
{
public:
    CacheAnimationShiftLeft(QWidget* widget, CacheSpace* cacheSpace)
        : CacheAnimationAbstract(widget, cacheSpace)
    {}

    QAbstractAnimation* createAnimation(QWidget* /*widget*/, CacheSpace* cacheSpace) override
    {
        auto animation = new QParallelAnimationGroup(this);
        // init
        cacheSpace->forEachCacheView([this, animation, cacheSpace](const CacheSpace::IterateInfo& info)->bool {
            auto ss = new QSequentialAnimationGroup(animation);

            auto va = new QVariantAnimation(ss);
            va->setDuration(1000);
            va->setEasingCurve(easingCurve());
            QRect startRect = info.cacheView->rect();
            startRect.moveTo(cacheSpace->window().right(), startRect.top());
            va->setStartValue(startRect);
            va->setEndValue(info.cacheView->rect());
            CacheView* cv = info.cacheView;
            connect(va, &QVariantAnimation::valueChanged, [cv](const QVariant &value){
                cv->rRect() = value.toRect();
            });
            ss->addPause(info.cacheItemIndex*100);
            ss->addAnimation(va);

            animation->addAnimation(ss);

            if (direction() == QAbstractAnimation::Forward)
            {
                info.cacheView->rRect() = startRect;
            }

            return true;
        });

        return animation;
    }
};

class CacheAnimationShiftRandom : public CacheAnimationAbstract
{
public:
    CacheAnimationShiftRandom(QWidget* widget, CacheSpace* cacheSpace)
        : CacheAnimationAbstract(widget, cacheSpace)
    {}

    QAbstractAnimation* createAnimation(QWidget* /*widget*/, CacheSpace* cacheSpace) override
    {
        auto animation = new QParallelAnimationGroup(this);
        QRect randomArea = cacheSpace->window();
        randomArea.adjust(-2*randomArea.width(), -2*randomArea.height(), 2*randomArea.width(), 2*randomArea.height());

        cacheSpace->forEachCacheView([this, &randomArea, animation, cacheSpace](const CacheSpace::IterateInfo& info)->bool {
            auto ss = new QSequentialAnimationGroup(animation);

            auto va = new QVariantAnimation(ss);
            va->setDuration(1000);
            va->setEasingCurve(easingCurve());
            QRect startRect;

            do
            {
                //startRect = info.cacheView->rect();
                QPoint randomPoint;
                randomPoint.rx() = randomArea.left() + std::rand()%randomArea.width();
                randomPoint.ry() = randomArea.top() + std::rand()%randomArea.height();
                QSize randomSize;
                //float factor = (float)(std::rand()%100)/100.f;
                randomSize.rwidth() = 1;//int(factor * (float)info.cacheView->rect().width());
                randomSize.rheight() = 1;//int(factor * (float)info.cacheView->rect().height());

                startRect = QRect(randomPoint, randomSize);
            } while (startRect.intersects(cacheSpace->window()));

            va->setStartValue(startRect);
            va->setEndValue(info.cacheView->rect());
            CacheView* cv = info.cacheView;
            connect(va, &QVariantAnimation::valueChanged, [cv](const QVariant &value){
                cv->rRect() = value.toRect();
            });
            ss->addPause(info.cacheItemIndex*100);
            ss->addAnimation(va);

            animation->addAnimation(ss);

            if (direction() == QAbstractAnimation::Forward)
            {
                info.cacheView->rRect() = startRect;
            }

            return true;
        });

        return animation;
    }
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_backAnimation(nullptr)
{
    ui->setupUi(this);

    m_resizer = QSharedPointer<Qi::ListColumnsResizer>::create(ui->listWidget);

    auto& grid = *ui->listWidget->grid();

    grid.rows()->setCount(20);
    grid.rows()->setLineSizeAll(150);
    grid.columns()->setCount(1);
    //grid.columns()->setLineSizeAll(500);

    {
        m_images = QSharedPointer<ModelStorageColumn<QPixmap>>::create(grid.rows());
        m_names = QSharedPointer<ModelStorageColumn<QString>>::create(grid.rows());
        m_descriptions = QSharedPointer<ModelStorageColumn<QString>>::create(grid.rows());

        m_images->setValue(0, 0, QPixmap("://img/afghan-hound_01_sm.jpg"));
        m_names->setValue(0, 0, "Afghan Hound");
        m_descriptions->setValue(0, 0, "The Afghan Hound is a hound that is one of the oldest dog breeds in existence. Distinguished by its thick, fine, silky coat and its tail with a ring curl at the end, the breed acquired its unique features in the cold mountains of Afghanistan.");

        m_images->setValue(1, 0, QPixmap("://img/alaskan-malamute_01_sm.jpg"));
        m_names->setValue(1, 0, "Alaskan Malamute");
        m_descriptions->setValue(1, 0, "The Alaskan Malamute is a large breed of domestic dog (Canis lupus familiaris) originally bred for hauling heavy freight because of their strength and endurance, and later an Alaskan sled dog.");

        m_images->setValue(2, 0, QPixmap("://img/american-foxhound_01_sm.jpg"));
        m_names->setValue(2, 0, "American Foxhound");
        m_descriptions->setValue(2, 0, "The American Foxhound is a breed of dog that is a cousin of the English Foxhound. They are scent hounds, bred to hunt foxes by scent.");

        m_images->setValue(3, 0, QPixmap("://img/anatolian-shepherd_01_sm.jpg"));
        m_names->setValue(3, 0, "Anatolian Shepherd");
        m_descriptions->setValue(3, 0, "The Anatolian Shepherd Dog (Turkish: Anadolu çoban köpeği) is a breed of dog which originated in Anatolia (central Turkey) and was further developed as a breed in America. It is rugged, large and very strong; with superior sight and hearing allowing it to protect livestock.");

        m_images->setValue(4, 0, QPixmap("://img/australian-shepherd_01_sm.jpg"));
        m_names->setValue(4, 0, "Australian Shepherd");
        m_descriptions->setValue(4, 0, "The Australian Shepherd, commonly known as the Aussie, is a medium size breed of dog that was developed on ranches in the western United States. Despite its name, the breed was not developed in Australia, but rather in the United States where they were seen in the West as early as the 1800s.");

        m_images->setValue(5, 0, QPixmap("://img/basenji_01_sm.jpg"));
        m_names->setValue(5, 0, "Basenji");
        m_descriptions->setValue(5, 0, "The Basenji is a breed of hunting dog that was bred from stock originating in central Africa. Most of the major kennel clubs in the English-speaking world place the breed in the Hound Group; more specifically, it may be classified as belonging to the sighthound type.");

        m_images->setValue(6, 0, QPixmap("://img/basset-hound_01_sm.jpg"));
        m_names->setValue(6, 0, "Basset Hound");
        m_descriptions->setValue(6, 0, "The Basset Hound is a short-legged breed of dog of the hound family, as well as one of six recognized Basset breeds in France; furthermore, Bassets are scent hounds that were originally bred for the purpose of hunting rabbits and hare.");

        m_images->setValue(7, 0, QPixmap("://img/beagle_01_sm.jpg"));
        m_names->setValue(7, 0, "Beagle");
        m_descriptions->setValue(7, 0, "The Beagle is a breed of small to medium-sized dog. A member of the hound group, it is similar in appearance to the foxhound, but smaller with shorter legs and longer, softer ears. Beagles are scent hounds, developed primarily for tracking hare, rabbit, deer, and other small game.");

        m_images->setValue(8, 0, QPixmap("://img/belgian-malinois_01_sm.jpg"));
        m_names->setValue(8, 0, "Malinois");
        m_descriptions->setValue(8, 0, "The Malinois or Belgian Shepherd Dog is a medium breed of dog, sometimes classified as a variety of the Belgian Shepherd Dog classification, rather than as a separate breed. The Malinois is recognized in the United States under the name Belgian Malinois.");

        m_images->setValue(9, 0, QPixmap("://img/borzoi_01_sm.jpg"));
        m_names->setValue(9, 0, "Borzoi");
        m_descriptions->setValue(9, 0, "The Borzoi, also called the Russian wolfhound, is a breed of domestic dog (Canis lupus familiaris). Descended from dogs brought to Russia from central Asian countries, it is similar in shape to a greyhound, and is also a member of the sighthound family.");

        m_images->setValue(10, 0, QPixmap("://img/bulldog_01_sm.jpg"));
        m_names->setValue(10, 0, "Bulldog");
        m_descriptions->setValue(10, 0, "The Bulldog is a medium-sized breed of dog commonly referred to as the English Bulldog or British Bulldog. The Bulldog is a muscular, heavy dog with a wrinkled face and a distinctive pushed-in nose.");

        m_images->setValue(11, 0, QPixmap("://img/vizsla_01_sm.jpg"));
        m_names->setValue(11, 0, "Vizsla");
        m_descriptions->setValue(11, 0, "The Vizsla is a dog breed originating in Hungary, which belongs under the FCI group 7 (Pointer group). The Hungarian or Magyar Vizsla are sporting dogs and loyal companions, in addition to being the smallest of the all-round pointer-retriever breeds.");

        m_images->setValue(12, 0, QPixmap("://img/dalmation_01_sm.jpg"));
        m_names->setValue(12, 0, "Dalmatian");
        m_descriptions->setValue(12, 0, "The Dalmatian is a large breed of dog noted for its unique black or liver spotted coat and was mainly used as a carriage dog in its early days. Its roots trace back to Croatia and its historical region of Dalmatia. Today, this dog remains a well-loved family pet, and many dog enthusiasts enter their pets into kennel club competitions.");

        m_images->setValue(13, 0, QPixmap("://img/english-setter_01_sm.jpg"));
        m_names->setValue(13, 0, "English Setter");
        m_descriptions->setValue(13, 0, "The English Setter is a medium size breed of dog. It is part of the Setter family, which includes the red Irish Setters, Irish Red and White Setters, and black-and-tan Gordon Setters. The mainly white body coat is of medium length with long silky fringes on the back of the legs, under the belly and on the tail.");

        m_images->setValue(14, 0, QPixmap("://img/greater-swiss-mountain-dog_01_sm.jpg"));
        m_names->setValue(14, 0, "Greater Swiss Mountain Dog");
        m_descriptions->setValue(14, 0, "The Greater Swiss Mountain Dog (German: Grosser Schweizer Sennenhund or French: Grand Bouvier Suisse) is a dog breed which was developed in the Swiss Alps. The name Sennenhund refers to people called Senn or Senner, dairymen and herders in the Swiss Alps.");

        m_images->setValue(15, 0, QPixmap("://img/irish-water-spaniel_01_sm.jpg"));
        m_names->setValue(15, 0, "Irish Water Spaniel");
        m_descriptions->setValue(15, 0, "The Irish Water Spaniel (Irish: An Spáinnéar Uisce) is a breed of dog that is the largest and one of the oldest of spaniels. The Irish Water Spaniel is one of the rarer breeds with the AKC in terms of registrations.");

        m_images->setValue(16, 0, QPixmap("://img/neopolitan-mastiff_01_sm.jpg"));
        m_names->setValue(16, 0, "Neapolitan Mastiff");
        m_descriptions->setValue(16, 0, "The Neapolitan Mastiff or Italian Mastiff, (Italian: Mastino Napoletano) is a large, ancient dog breed. This massive breed is often used as a guard and defender of family and property due to their protective instincts and their fearsome appearance.");

        m_images->setValue(17, 0, QPixmap("://img/poodle-standard_01_sm.jpg"));
        m_names->setValue(17, 0, "Poodle");
        m_descriptions->setValue(17, 0, "The poodle is a group of formal dog breeds, the Standard Poodle, Miniature Poodle and Toy Poodle (one registry organisation also recognizes a Medium Poodle variety, between Standard and Miniature), with many coat colors. Originally bred in Germany as a type of water dog, the breed was standardized in France.");

        m_images->setValue(18, 0, QPixmap("://img/shiba-inu_01_sm.jpg"));
        m_names->setValue(18, 0, "Shiba Inu");
        m_descriptions->setValue(18, 0, "The Shiba Inu is the smallest of the six original and distinct spitz breeds of dog from Japan. A small, agile dog that copes very well with mountainous terrain, the Shiba Inu was originally bred for hunting.");

        m_images->setValue(19, 0, QPixmap("://img/siberian-husky_01_sm.jpg"));
        m_names->setValue(19, 0, "Siberian Husky");
        m_descriptions->setValue(19, 0, "The Siberian Husky is a medium size, dense-coat working dog breed that originated in north-eastern Siberia. The breed belongs to the Spitz genetic family. It is recognizable by its thickly furred double coat, erect triangular ears, and distinctive markings.");
    }

    // setup alternate background
    //grid.addSchema(makeRangeAll(), QSharedPointer<ViewAlternateBackground>::create(), makeLayoutBackground());

    QFont boldFont = ui->listWidget->font();
    boldFont.setBold(true);

    auto viewImages = QSharedPointer<ViewPixmap>::create(m_images);

    grid.addSchema(makeRangeColumn(0), QSharedPointer<ViewRowBorder>::create(), makeLayoutBackground());
    grid.addSchema(makeRangeColumn(0), viewImages, makeLayoutLeft());

    {
        QVector<ViewSchema> subViews;
        subViews.append(ViewSchema(makeLayoutBackground(), QSharedPointer<ViewTextFont>::create(boldFont)));
        subViews.append(ViewSchema(makeLayoutClient(), QSharedPointer<ViewText>::create(m_names, ViewDefaultControllerNone, Qt::AlignHCenter|Qt::AlignTop, Qt::ElideRight)));
        grid.addSchema(makeRangeColumn(0), QSharedPointer<ViewComposite>::create(subViews), makeLayoutTop());
    }

    grid.addSchema(makeRangeColumn(0), QSharedPointer<ViewText>::create(m_descriptions, ViewDefaultControllerNone, Qt::Alignment(Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap)), makeLayoutClient());

    {
        auto wikiModel = QSharedPointer<ModelCallback<QUrl>>::create();
        wikiModel->getValueFunction = [this](const ItemID& item)->QUrl {
            return QUrl(QString("http://en.wikipedia.org/wiki/%1").arg(m_names->value(item)));
        };
        auto wikiView = QSharedPointer<ViewLink>::create(QSharedPointer<ModelStorageValue<QString>>::create("more"), wikiModel);
        QVector<ViewSchema> subViews;
        subViews.append(ViewSchema(makeLayoutRight(), wikiView));
        grid.addSchema(makeRangeColumn(0), QSharedPointer<ViewComposite>::create(subViews), makeLayoutBottom());
    }

    // animation
    m_backAnimation = new CacheAnimationShiftRight(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
    m_backAnimation->setEasingCurve(QEasingCurve::OutCirc);
    m_backAnimation->setDirection(QAbstractAnimation::Backward);

    auto a = new CacheAnimationShiftRight(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
    a->setEasingCurve(QEasingCurve::OutCirc);
    a->launch();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::shuffleRows()
{
    QVector<int> permutation = ui->listWidget->grid()->rows()->permutation();
    std::random_shuffle(permutation.begin(), permutation.end());
    ui->listWidget->grid()->rows()->setPermutation(permutation);
}

void MainWindow::on_pushButton_clicked()
{
    close();
}

void MainWindow::on_shiftRightBttn_clicked()
{
    connect(m_backAnimation, &QAbstractAnimation::finished, [this]() {
        shuffleRows();

        auto a = new CacheAnimationShiftRight(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
        a->setEasingCurve(QEasingCurve::OutCirc);
        a->launch();
    });
    m_backAnimation->launch();

    m_backAnimation = new CacheAnimationShiftRight(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
    m_backAnimation->setEasingCurve(QEasingCurve::OutCirc);
    m_backAnimation->setDirection(QAbstractAnimation::Backward);
}

void MainWindow::on_shiftLeftBttn_clicked()
{
    connect(m_backAnimation, &QAbstractAnimation::finished, [this]() {
        shuffleRows();

        auto a = new CacheAnimationShiftLeft(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
        a->setEasingCurve(QEasingCurve::OutCirc);
        a->launch();
    });
    m_backAnimation->launch();

    m_backAnimation = new CacheAnimationShiftLeft(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
    m_backAnimation->setEasingCurve(QEasingCurve::OutCirc);
    m_backAnimation->setDirection(QAbstractAnimation::Backward);
}

void MainWindow::on_shiftRandomBttn_clicked()
{
    connect(m_backAnimation, &QAbstractAnimation::finished, [this]() {
        shuffleRows();

        auto a = new CacheAnimationShiftRandom(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
        a->setEasingCurve(QEasingCurve::OutCirc);
        a->launch();
    });
    m_backAnimation->launch();

    m_backAnimation = new CacheAnimationShiftRandom(ui->listWidget->viewport(), ui->listWidget->cacheGrid().data());
    m_backAnimation->setEasingCurve(QEasingCurve::OutCirc);
    m_backAnimation->setDirection(QAbstractAnimation::Backward);
}
