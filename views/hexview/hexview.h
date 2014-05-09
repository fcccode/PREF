#ifndef HEXVIEW_H
#define HEXVIEW_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include "actionwidget/actiontoolbar.h"
#include "viewmodels/formatmodel/formatmodel.h"
#include "views/abstractview.h"
#include "views/hexview/optionmenu.h"
#include "datatypesview/datatypesview.h"
#include "prefsdk/sdkmanager.h"
#include "formatsdialog.h"

using namespace PrefSDK;

namespace Ui {
class HexView;
}

class HexView : public AbstractView
{
    Q_OBJECT
    
    public:
        explicit HexView(QHexEditData* hexeditdata, QLabel* labelinfo, QWidget *parent = 0);
        bool loadFormat(FormatList::Format &format, int64_t baseoffset);
        void scanSignatures(bool canscan);
        void save(QString filename);
        void save();
        ~HexView();

    public: /* Overriden Methods */
        virtual bool canSave() const;
        virtual void updateStatusBar();

    protected:
        virtual void closeEvent(QCloseEvent* event);

    private:
        void createToolBar();

    private slots:
        void updateOffset(qint64);
        void updateSelLength(qint64 size);
        void onLoadFormatClicked();
        void onSignatureScannerClicked();
        void onByteViewClicked();
        void onHexEditCustomContextMenuRequested(const QPoint& pos);
        void onSetBackColor(FormatElement *formatelement);
        void onRemoveBackColor(FormatElement *formatelement);
        void onFormatObjectSelected(FormatElement* formatelement);
        void exportData(FormatElement* formatelement);
        void importData(FormatElement *formatelement);
        void scanSignatures();

    private:
        Ui::HexView *ui;
        FormatList::FormatId _formatid;
        FormatModel* _formatmodel;
        FormatTree* _formattree;
        QHexEditData* _hexeditdata;
        ActionToolBar* _toolbar;
        QToolButton* _tbloadformat;
        QToolButton* _tbscansignature;
        QToolButton* _tbbyteview;
        QToolButton* _tbformatoptions;
        QColor _signaturecolor;
        bool _signscanenabled;
        bool _entropyenabled;
};

#endif // HEXVIEW_H
