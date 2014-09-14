/*  This file is part of the Kate project.
 *  Based on the snippet plugin from KDevelop 4.
 *
 *  Copyright (C) 2007 Robert Gruber <rgruber@users.sourceforge.net> 
 *  Copyright (C) 2012 Christoph Cullmann <cullmann@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#include "katesnippetglobal.h"

#include "snippetview.h"
#include "snippetcompletionmodel.h"
#include "snippetstore.h"
#include "snippet.h"
#include "snippetrepository.h"
#include "snippetcompletionitem.h"
#include "editsnippet.h"

#include <KPluginFactory>
#include <KAboutData>
#include <KPluginLoader>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <ktexteditor/editor.h>
#include <ktexteditor/application.h>
#include <ktexteditor/mainwindow.h>
#include <ktexteditor/codecompletioninterface.h>
#include <KToolBar>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QMenu>

KateSnippetGlobal *KateSnippetGlobal::s_self = nullptr;

KateSnippetGlobal::KateSnippetGlobal(QObject *parent, const QVariantList &)
  : QObject(parent)
{
    s_self = this;

    SnippetStore::init(this);
    m_model.reset(new SnippetCompletionModel);
}

KateSnippetGlobal::~KateSnippetGlobal ()
{
    delete SnippetStore::self();
    s_self = nullptr;
}

void KateSnippetGlobal::showDialog (KTextEditor::View *view)
{
  QDialog dialog;
  dialog.setWindowTitle(i18n("Snippets"));

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  dialog.setLayout(layout);

  KToolBar *topToolbar = new KToolBar(&dialog, "snippetsToolBar");
  topToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  layout->addWidget(topToolbar);

  QWidget *widget = snippetWidget();
  layout->addWidget(widget);

  // add actions
  topToolbar->addActions(widget->actions());

  QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
  layout->addWidget(buttons);
  buttons->setStandardButtons(QDialogButtonBox::Ok);
  connect(buttons, SIGNAL(accepted()), &dialog, SLOT(close()));

  /**
   * set document to work on and trigger dialog
   */
  m_activeViewForDialog = view;
  dialog.exec();
  m_activeViewForDialog = 0;
}

SnippetView* KateSnippetGlobal::snippetWidget ()
{
  return new SnippetView (this, 0);
}

void KateSnippetGlobal::insertSnippet(Snippet* snippet)
{
  // query active view, always prefer that!
  KTextEditor::View *view = KTextEditor::Editor::instance()->application()->activeMainWindow()->activeView ();
  
  // fallback to stuff set for dialog
  if (!view)
    view = m_activeViewForDialog;
 
  // no view => nothing to do
  if (!view)
    return;

  // try to insert snippet
  SnippetCompletionItem item(snippet, static_cast<SnippetRepository*>(snippet->parent()));
  item.execute(view, KTextEditor::Range(view->cursorPosition(), view->cursorPosition()));

  // set focus to view
  view->setFocus ();
}

void KateSnippetGlobal::insertSnippetFromActionData()
{
    QAction* action = dynamic_cast<QAction*>(sender());
    Q_ASSERT(action);
    Snippet* snippet = action->data().value<Snippet*>();
    Q_ASSERT(snippet);
    insertSnippet(snippet);
}

void KateSnippetGlobal::createSnippet (KTextEditor::View *view)
{
    // no active view, bad
    if (!view)
        return;

    // get mode
    QString mode = view->document()->highlightingModeAt(view->selectionRange().isValid() ? view->selectionRange().start() : view->cursorPosition());
    if ( mode.isEmpty() )
        mode = view->document()->mode();

    // try to look for a fitting repo
    SnippetRepository* match = 0;
    for ( int i = 0; i < SnippetStore::self()->rowCount(); ++i ) {
        SnippetRepository* repo = dynamic_cast<SnippetRepository*>( SnippetStore::self()->item(i) );
        if ( repo && repo->fileTypes().count() == 1 && repo->fileTypes().first() == mode ) {
            match = repo;
            break;
        }
    }
    bool created = !match;
    if ( created ) {
        match = SnippetRepository::createRepoFromName(
                i18nc("Autogenerated repository name for a programming language",
                      "%1 snippets", mode)
        );
        match->setFileTypes(QStringList() << mode);
    }

    EditSnippet dlg(match, 0, view);
    dlg.setSnippetText(view->selectionText());
    int status = dlg.exec();
    if ( created && status != QDialog::Accepted ) {
        // cleanup
        match->remove();
    }
}
