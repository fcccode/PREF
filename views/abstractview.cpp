#include "abstractview.h"

AbstractView::AbstractView(QHexEditData* hexeditdata, const QString &loadedfile, QLabel* labelinfo, QWidget *parent): QWidget(parent), _hexeditdata(hexeditdata), _lblinfo(labelinfo), _loadedfile(loadedfile)
{
    hexeditdata->setParent(this); /* Take Ownership */

    if(!loadedfile.isEmpty())
    {
        QFileInfo fi(loadedfile);
        QDir::setCurrent(fi.absolutePath());
    }
}

AbstractView::~AbstractView()
{

}

bool AbstractView::canSaveAs() const
{
    return this->canSave();
}

QString AbstractView::saveFilter() const
{
    return "All Files (*.*)";
}

const QString &AbstractView::loadedFile() const
{
    return this->_loadedfile;
}

void AbstractView::saveAs()
{

}

void AbstractView::save()
{

}

void AbstractView::updateInfoText(const QString &s)
{
    this->_lblinfo->setText(s);
}
