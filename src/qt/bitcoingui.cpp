/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011
 */
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookdialog.h"
#include "sendcoinsdialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "guiutil.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>

#include <QDebug>

#include <iostream>

BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    trayIcon(0)
{
    resize(850, 550);
    setWindowTitle(tr("Devcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));

    createActions();

    // Menus
    QMenu *file = menuBar()->addMenu("&File");
    file->addAction(sendcoins);
    file->addSeparator();
    file->addAction(quit);
    
    QMenu *settings = menuBar()->addMenu("&Settings");
    settings->addAction(receivingAddresses);
    settings->addAction(options);

    QMenu *help = menuBar()->addMenu("&Help");
    help->addAction(about);
    
    // Toolbar
    QToolBar *toolbar = addToolBar("Main toolbar");
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(sendcoins);
    toolbar->addAction(addressbook);
    toolbar->addAction(receivingAddresses);

    // Balance: <balance>
    QHBoxLayout *hbox_balance = new QHBoxLayout();
    hbox_balance->addWidget(new QLabel(tr("Balance:")));
    hbox_balance->addSpacing(5);/* Add some spacing between the label and the text */

    labelBalance = new QLabel();
    labelBalance->setFont(QFont("Monospace", -1, QFont::Bold));
    labelBalance->setToolTip(tr("Your current balance"));
    hbox_balance->addWidget(labelBalance);
    hbox_balance->addStretch(1);
    
    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->addLayout(hbox_balance);

    transactionView = new TransactionView(this);
    connect(transactionView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(transactionDetails(const QModelIndex&)));
    vbox->addWidget(transactionView);

    QWidget *centralwidget = new QWidget(this);
    centralwidget->setLayout(vbox);
    setCentralWidget(centralwidget);
    
    // Create status bar
    statusBar();

    labelConnections = new QLabel();
    labelConnections->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    labelConnections->setMinimumWidth(150);
    labelConnections->setToolTip(tr("Number of connections to other clients"));

    labelBlocks = new QLabel();
    labelBlocks->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    labelBlocks->setMinimumWidth(130);
    labelBlocks->setToolTip(tr("Number of blocks in the block chain"));

    labelTransactions = new QLabel();
    labelTransactions->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    labelTransactions->setMinimumWidth(130);
    labelTransactions->setToolTip(tr("Number of transactions in your wallet"));

    // Progress bar for blocks download
    progressBarLabel = new QLabel(tr("Synchronizing with network..."));
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    progressBar->setToolTip(tr("Block chain synchronization in progress"));
    progressBar->setVisible(false);

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(labelConnections);
    statusBar()->addPermanentWidget(labelBlocks);
    statusBar()->addPermanentWidget(labelTransactions);

    createTrayIcon();
}

void BitcoinGUI::createActions()
{
    quit = new QAction(QIcon(":/icons/quit"), tr("&Exit"), this);
    quit->setToolTip(tr("Quit application"));
    sendcoins = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendcoins->setToolTip(tr("Send coins to a devcoin address"));
    addressbook = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressbook->setToolTip(tr("Edit the list of stored addresses and labels"));
    about = new QAction(QIcon(":/icons/bitcoin"), tr("&About"), this);
    about->setToolTip(tr("Show information about Devcoin"));
    receivingAddresses = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receiving Addresses..."), this);
    receivingAddresses->setToolTip(tr("Show the list of addresses for receiving payments"));
    options = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    options->setToolTip(tr("Modify configuration options for devcoin"));
    openBitcoin = new QAction(QIcon(":/icons/bitcoin"), "Open &Devcoin", this);
    openBitcoin->setToolTip(tr("Show the Devcoin window"));

    connect(quit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(sendcoins, SIGNAL(triggered()), this, SLOT(sendcoinsClicked()));
    connect(addressbook, SIGNAL(triggered()), this, SLOT(addressbookClicked()));
    connect(receivingAddresses, SIGNAL(triggered()), this, SLOT(receivingAddressesClicked()));
    connect(options, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(about, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(openBitcoin, SIGNAL(triggered()), this, SLOT(show()));
}

void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;

    if(clientModel->isTestNet())
    {
        setWindowTitle(tr("Devcoin [testnet]"));
        setWindowIcon(QIcon(":icons/bitcoin_testnet"));
        if(trayIcon)
        {
            trayIcon->setToolTip(tr("Devcoin [testnet]"));
            trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
        }
    }

    // Keep up to date with client
    setNumConnections(clientModel->getNumConnections());
    connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

    setNumBlocks(clientModel->getNumBlocks());
    connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));

    // Report errors from network/worker thread
    connect(clientModel, SIGNAL(error(QString,QString)), this, SLOT(error(QString,QString)));
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;

    // Keep up to date with wallet
    setBalance(walletModel->getBalance());
    connect(walletModel, SIGNAL(balanceChanged(qint64)), this, SLOT(setBalance(qint64)));

    setNumTransactions(walletModel->getNumTransactions());
    connect(walletModel, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

    // Report errors from wallet thread
    connect(walletModel, SIGNAL(error(QString,QString)), this, SLOT(error(QString,QString)));

    // Put transaction list in tabs
    transactionView->setModel(walletModel->getTransactionTableModel());

    // Balloon popup for new transaction
    connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(const QModelIndex &, int, int)),
            this, SLOT(incomingTransaction(const QModelIndex &, int, int)));
}

void BitcoinGUI::createTrayIcon()
{
    QMenu *trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(openBitcoin);
    trayIconMenu->addAction(sendcoins);
    trayIconMenu->addAction(options);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip("Devcoin client");
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
}

void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::DoubleClick)
    {
        // Doubleclick on system tray icon triggers "open devcoin"
        openBitcoin->trigger();
    }
}

void BitcoinGUI::sendcoinsClicked()
{
    SendCoinsDialog dlg;
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::addressbookClicked()
{
    AddressBookDialog dlg(AddressBookDialog::ForEditing);
    dlg.setModel(walletModel->getAddressTableModel());
    dlg.setTab(AddressBookDialog::SendingTab);
    dlg.exec();
}

void BitcoinGUI::receivingAddressesClicked()
{
    AddressBookDialog dlg(AddressBookDialog::ForEditing);
    dlg.setModel(walletModel->getAddressTableModel());
    dlg.setTab(AddressBookDialog::ReceivingTab);
    dlg.exec();
}

void BitcoinGUI::optionsClicked()
{
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::setBalance(qint64 balance)
{
    labelBalance->setText(GUIUtil::formatMoney(balance) + QString(" DVC"));
}

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnections->setTextFormat(Qt::RichText);
    labelConnections->setText("<img src=\""+icon+"\"> " + tr("%n connection(s)", "", count));
}

void BitcoinGUI::setNumBlocks(int count)
{
    int total = clientModel->getTotalBlocksEstimate();
    if(count < total)
    {
        progressBarLabel->setVisible(true);
        progressBar->setVisible(true);
        progressBar->setMaximum(total);
        progressBar->setValue(count);
    }
    else
    {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }

    labelBlocks->setText(tr("%n block(s)", "", count));
}

void BitcoinGUI::setNumTransactions(int count)
{
    labelTransactions->setText(tr("%n transaction(s)", "", count));
}

void BitcoinGUI::error(const QString &title, const QString &message)
{
    // Report errors from network/worker thread
    if(trayIcon->supportsMessages())
    {
        // Show as "balloon" message if possible
        trayIcon->showMessage(title, message, QSystemTrayIcon::Critical);
    }
    else
    {
        // Fall back to old fashioned popup dialog if not
        QMessageBox::critical(this, title,
            message,
            QMessageBox::Ok, QMessageBox::Ok);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::WindowStateChange)
    {
        if(clientModel->getOptionsModel()->getMinimizeToTray())
        {
            if (isMinimized())
            {
                hide();
                e->ignore();
            }
            else
            {
                e->accept();
            }
        }
    }
    QMainWindow::changeEvent(e);
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
       !clientModel->getOptionsModel()->getMinimizeOnClose())
    {
        qApp->quit();
    }
    QMainWindow::closeEvent(event);
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
          "which goes to the nodes that process your transaction and helps to support the network.  "
          "Do you want to pay the fee?").arg(GUIUtil::formatMoney(nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("Sending..."), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::transactionDetails(const QModelIndex& idx)
{
    // A transaction is doubleclicked
    TransactionDescDialog dlg(idx);
    dlg.exec();
}

void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();
    if(amount>0 && !clientModel->inInitialBlockDownload())
    {
        // On incoming transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
                        .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
                        .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
                        .data().toString();

        trayIcon->showMessage(tr("Incoming transaction"),
                              tr("Date: ") + date + "\n" +
                              tr("Amount: ") + GUIUtil::formatMoney(amount, true) + "\n" +
                              tr("Type: ") + type + "\n" +
                              tr("Address: ") + address + "\n",
                              QSystemTrayIcon::Information);
    }
}
