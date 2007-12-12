/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */



#include <QtGui/QApplication>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QTableWidget>
#include <QtGui/QPushButton>
#include <QtGui/QTabWidget>
#include <QtGui/QTextEdit>

#include "AboutDlg.h"
#include "AppConfig.h"
#include "AppVersion.h"
#include "ConfigurationSettingsImp.h"
#include "DateTime.h"
#include "Icons.h"
#include "LabeledSection.h"
#include "LabeledSectionGroup.h"
#include "PlugInBranding.h"
#include "PlugInDescriptor.h"
#include "PlugInManagerServices.h"
#include "Service.h"
#include "StringUtilities.h"

#include <string>
using namespace std;

AboutDlg::AboutDlg(QWidget* parent) :
   QDialog(parent)
{
   // Get the version and date values
   QString strVersion;
   QString strReleaseDate;
   bool bProductionRelease = false;
   QString strReleaseType;

   ConfigurationSettingsImp* pConfigSettings = dynamic_cast<ConfigurationSettingsImp*>(Service<ConfigurationSettings>().get());
   strVersion = QString::fromStdString(pConfigSettings->getVersion());
   strVersion += " Build " + QString::fromStdString(pConfigSettings->getBuildRevision());

   const DateTime* pReleaseDate = NULL;
   pReleaseDate = pConfigSettings->getReleaseDate();
   if (pReleaseDate != NULL)
   {
      string format = "%d %b %Y";
      strReleaseDate = QString::fromStdString(pReleaseDate->getFormattedUtc(format));
   }

   bProductionRelease = pConfigSettings->isProductionRelease();
   strReleaseType = QString::fromStdString(
      StringUtilities::toDisplayString(pConfigSettings->getReleaseType()));

   QFont ftApp = QApplication::font();
   ftApp.setBold(true);

   LabeledSectionGroup* pAboutGroup = new LabeledSectionGroup(this);

   QWidget* pAppInfo = new QWidget(this);

   // Version
   QLabel* pVersionLabel = new QLabel("Version:", pAppInfo);
   pVersionLabel->setFont(ftApp);

   QLabel* pVersion = new QLabel(strVersion, pAppInfo);

   // Release date
   QLabel* pReleaseLabel = new QLabel("Release Date:", pAppInfo);
   pReleaseLabel->setFont(ftApp);

   QLabel* pRelease = new QLabel(strReleaseDate, pAppInfo);


   // Release information
   QLabel* pInfoLabel = NULL;
   if (bProductionRelease == false)
   {
      pInfoLabel = new QLabel(strReleaseType + " - Not for Production Use", pAppInfo);
      pInfoLabel->setFont(ftApp);

      QPalette labelPalette = pInfoLabel->palette();
      labelPalette.setColor(QPalette::WindowText, Qt::red);
      pInfoLabel->setPalette(labelPalette);
   }

   QLabel* pIconLabel = new QLabel(pAppInfo);
   Icons* pIcons = Icons::instance();
   if (pIcons != NULL)
   {
      pIconLabel->setPixmap(pIcons->mApplicationLarge);
   }

   // Copyright
   QLabel* pCopyrightLabel = new QLabel(APP_COPYRIGHT, pAppInfo);
   pCopyrightLabel->setAlignment(Qt::AlignLeft);
   pCopyrightLabel->setWordWrap(true);

   QGridLayout* pTextGrid = new QGridLayout(pAppInfo);
   pTextGrid->setMargin(0);
   pTextGrid->setSpacing(5);
   pTextGrid->addWidget(pIconLabel, 0, 0, 5, 1, Qt::AlignVCenter);
   pTextGrid->addWidget(pVersionLabel, 0, 1);
   pTextGrid->addWidget(pVersion, 0, 2);
   pTextGrid->addWidget(pReleaseLabel, 1, 1);
   pTextGrid->addWidget(pRelease, 1, 2);

   if ((bProductionRelease == false) && (pInfoLabel != NULL))
   {
      pTextGrid->addWidget(pInfoLabel, 2, 1, 1, 2);
   }

   pTextGrid->setRowMinimumHeight(3, 10);
   pTextGrid->addWidget(pCopyrightLabel, 4, 1, 1, 2);
   pTextGrid->setColumnStretch(2, 10);
   pTextGrid->setColumnMinimumWidth(0, 75);

   LabeledSection* pAppInfoSection = new LabeledSection(pAppInfo, APP_NAME_LONG " Version Information");
   pAboutGroup->addSection(pAppInfoSection);

   //Plug-In Suites listing
   const vector<PlugInBranding>& brandings = PlugInBranding::getBrandings();
   QTableWidget* pPlugInBrandingTable = new QTableWidget(this);
   pPlugInBrandingTable->setColumnCount(3);
   pPlugInBrandingTable->setRowCount(brandings.size());
   pPlugInBrandingTable->setSelectionMode(QAbstractItemView::NoSelection);
   pPlugInBrandingTable->verticalHeader()->hide();
   pPlugInBrandingTable->setHorizontalHeaderLabels(QStringList() << "Name" << "Description" << "Version");
   pPlugInBrandingTable->horizontalHeader()->setClickable(false);
   pPlugInBrandingTable->horizontalHeader()->setResizeMode(1, QHeaderView::Stretch);
   pPlugInBrandingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
   for (unsigned int itemCount = 0;
        itemCount < brandings.size();
        ++itemCount)
   {
      QTableWidgetItem* pTitle = new QTableWidgetItem(QString::fromStdString(brandings[itemCount].getTitle()));
      pTitle->setToolTip(pTitle->text());
      pPlugInBrandingTable->setItem(itemCount, 0, pTitle);
      QTableWidgetItem* pDesc = new QTableWidgetItem(QString::fromStdString(brandings[itemCount].getDescription()));
      pDesc->setToolTip(pDesc->text());
      pPlugInBrandingTable->setItem(itemCount, 1, pDesc);
      QTableWidgetItem* pVersion = new QTableWidgetItem(QString::fromStdString(brandings[itemCount].getVersion()));
      pVersion->setToolTip(pVersion->text());
      pPlugInBrandingTable->setItem(itemCount, 2, pVersion);
   }
   pPlugInBrandingTable->resizeColumnToContents(0);
   pPlugInBrandingTable->resizeColumnToContents(2);
   pPlugInBrandingTable->sortItems(0);

   LabeledSection* pPlugInSuiteSection = new LabeledSection(pPlugInBrandingTable, "Plug-In Suites");
   pAboutGroup->addSection(pPlugInSuiteSection, 1000);

   // Create the copyright notice box
   QString strNotice = QString::fromStdString(APP_NAME) + " is " 
      "subject to the terms and conditions of the "
      "GNU Lesser General Public License Version 2.1.  Installed plug-ins may be subject to "
      "different license terms.<br><br>"
      "GNU LESSER GENERAL PUBLIC LICENSE<br>"
      "Version 2.1, February 1999<br>"
      "<br>"
      "Copyright (C) 1991, 1999 Free Software Foundation, Inc.<br>"
      "51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA<br>"
      "Everyone is permitted to copy and distribute verbatim copies "
      "of this license document, but changing it is not allowed.<br>"
      "<br>"
      "[This is the first released version of the Lesser GPL.  It also counts "
      "as the successor of the GNU Library Public License, version 2, hence "
      "the version number 2.1.]<br>"
      "<br>"
      "Preamble<br>"
      "<br>"
      "The licenses for most software are designed to take away your "
      "freedom to share and change it.  By contrast, the GNU General Public "
      "Licenses are intended to guarantee your freedom to share and change "
      "free software--to make sure the software is free for all its users.<br>"
      "<br>"
      "This license, the Lesser General Public License, applies to some "
      "specially designated software packages--typically libraries--of the "
      "Free Software Foundation and other authors who decide to use it.  You "
      "can use it too, but we suggest you first think carefully about whether "
      "this license or the ordinary General Public License is the better "
      "strategy to use in any particular case, based on the explanations below.<br>"
      "<br>"
      "When we speak of free software, we are referring to freedom of use, "
      "not price.  Our General Public Licenses are designed to make sure that "
      "you have the freedom to distribute copies of free software (and charge "
      "for this service if you wish); that you receive source code or can get "
      "it if you want it; that you can change the software and use pieces of "
      "it in new free programs; and that you are informed that you can do "
      "these things.<br>"
      "<br>"
      "To protect your rights, we need to make restrictions that forbid "
      "distributors to deny you these rights or to ask you to surrender these "
      "rights.  These restrictions translate to certain responsibilities for "
      "you if you distribute copies of the library or if you modify it.<br>"
      "<br>"
      "For example, if you distribute copies of the library, whether gratis "
      "or for a fee, you must give the recipients all the rights that we gave "
      "you.  You must make sure that they, too, receive or can get the source "
      "code.  If you link other code with the library, you must provide "
      "complete object files to the recipients, so that they can relink them "
      "with the library after making changes to the library and recompiling "
      "it.  And you must show them these terms so they know their rights.<br>"
      "<br>"
      "We protect your rights with a two-step method: (1) we copyright the "
      "library, and (2) we offer you this license, which gives you legal "
      "permission to copy, distribute and/or modify the library.<br>"
      "<br>"
      "To protect each distributor, we want to make it very clear that "
      "there is no warranty for the free library.  Also, if the library is "
      "modified by someone else and passed on, the recipients should know "
      "that what they have is not the original version, so that the original "
      "author's reputation will not be affected by problems that might be "
      "introduced by others. <br>"
      "<br>"
      "Finally, software patents pose a constant threat to the existence of "
      "any free program.  We wish to make sure that a company cannot "
      "effectively restrict the users of a free program by obtaining a "
      "restrictive license from a patent holder.  Therefore, we insist that "
      "any patent license obtained for a version of the library must be "
      "consistent with the full freedom of use specified in this license.<br>"
      "<br>"
      "Most GNU software, including some libraries, is covered by the "
      "ordinary GNU General Public License.  This license, the GNU Lesser "
      "General Public License, applies to certain designated libraries, and "
      "is quite different from the ordinary General Public License.  We use "
      "this license for certain libraries in order to permit linking those "
      "libraries into non-free programs.<br>"
      "<br>"
      "When a program is linked with a library, whether statically or using "
      "a shared library, the combination of the two is legally speaking a "
      "combined work, a derivative of the original library.  The ordinary "
      "General Public License therefore permits such linking only if the "
      "entire combination fits its criteria of freedom.  The Lesser General "
      "Public License permits more lax criteria for linking other code with "
      "the library.<br>"
      "<br>"
      "We call this license the \"Lesser\" General Public License because it "
      "does Less to protect the user's freedom than the ordinary General "
      "Public License.  It also provides other free software developers Less "
      "of an advantage over competing non-free programs.  These disadvantages "
      "are the reason we use the ordinary General Public License for many "
      "libraries.  However, the Lesser license provides advantages in certain "
      "special circumstances.<br>"
      "<br>"
      "For example, on rare occasions, there may be a special need to "
      "encourage the widest possible use of a certain library, so that it becomes "
      "a de-facto standard.  To achieve this, non-free programs must be "
      "allowed to use the library.  A more frequent case is that a free "
      "library does the same job as widely used non-free libraries.  In this "
      "case, there is little to gain by limiting the free library to free "
      "software only, so we use the Lesser General Public License. "
      "<br><br>"
      "In other cases, permission to use a particular library in non-free "
      "programs enables a greater number of people to use a large body of "
      "free software.  For example, permission to use the GNU C Library in "
      "non-free programs enables many more people to use the whole GNU "
      "operating system, as well as its variant, the GNU/Linux operating "
      "system. "
      "<br><br>"
      "Although the Lesser General Public License is Less protective of the "
      "users' freedom, it does ensure that the user of a program that is "
      "linked with the Library has the freedom and the wherewithal to run "
      "that program using a modified version of the Library. "
      "<br><br>"
      "The precise terms and conditions for copying, distribution and "
      "modification follow.  Pay close attention to the difference between a "
      "\"work based on the library\" and a \"work that uses the library\".  The "
      "former contains code derived from the library, whereas the latter must "
      "be combined with the library in order to run. "
      "<br><br>"
      "GNU LESSER GENERAL PUBLIC LICENSE "
      "TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION "
      "<br><br>"
      "0. This License Agreement applies to any software library or other "
      "program which contains a notice placed by the copyright holder or "
      "other authorized party saying it may be distributed under the terms of "
      "this Lesser General Public License (also called \"this License\"). "
      "Each licensee is addressed as \"you\". "
      "<br><br>"
      "A \"library\" means a collection of software functions and/or data "
      "prepared so as to be conveniently linked with application programs "
      "(which use some of those functions and data) to form executables. "
      "<br><br>"
      "The \"Library\", below, refers to any such software library or work "
      "which has been distributed under these terms.  A \"work based on the "
      "Library\" means either the Library or any derivative work under "
      "copyright law: that is to say, a work containing the Library or a "
      "portion of it, either verbatim or with modifications and/or translated "
      "straightforwardly into another language.  (Hereinafter, translation is "
      "included without limitation in the term \"modification\".) "
      "<br><br>"
      "\"Source code\" for a work means the preferred form of the work for "
      "making modifications to it.  For a library, complete source code means "
      "all the source code for all modules it contains, plus any associated "
      "interface definition files, plus the scripts used to control compilation "
      "and installation of the library. "
      "<br><br>"
      "Activities other than copying, distribution and modification are not "
      "covered by this License; they are outside its scope.  The act of "
      "running a program using the Library is not restricted, and output from "
      "such a program is covered only if its contents constitute a work based "
      "on the Library (independent of the use of the Library in a tool for "
      "writing it).  Whether that is true depends on what the Library does "
      "and what the program that uses the Library does. "
      "<br><br>"
      "1. You may copy and distribute verbatim copies of the Library's "
      "complete source code as you receive it, in any medium, provided that "
      "you conspicuously and appropriately publish on each copy an "
      "appropriate copyright notice and disclaimer of warranty; keep intact "
      "all the notices that refer to this License and to the absence of any "
      "warranty; and distribute a copy of this License along with the "
      "Library. "
      "<br><br>"
      "You may charge a fee for the physical act of transferring a copy, "
      "and you may at your option offer warranty protection in exchange for a "
      "fee. "
      "<br><br>"
      "2. You may modify your copy or copies of the Library or any portion "
      "of it, thus forming a work based on the Library, and copy and "
      "distribute such modifications or work under the terms of Section 1 "
      "above, provided that you also meet all of these conditions: "
      "<br><br>"
      "a) The modified work must itself be a software library. "
      "<br><br>"
      "b) You must cause the files modified to carry prominent notices "
      "stating that you changed the files and the date of any change. "
      "<br><br>"
      "c) You must cause the whole of the work to be licensed at no "
      "charge to all third parties under the terms of this License. "
      "<br><br>"
      "d) If a facility in the modified Library refers to a function or a "
      "table of data to be supplied by an application program that uses "
      "the facility, other than as an argument passed when the facility "
      "is invoked, then you must make a good faith effort to ensure that, "
      "in the event an application does not supply such function or "
      "table, the facility still operates, and performs whatever part of "
      "its purpose remains meaningful. "
      "<br><br>"
      "(For example, a function in a library to compute square roots has "
      "a purpose that is entirely well-defined independent of the "
      "application.  Therefore, Subsection 2d requires that any "
      "application-supplied function or table used by this function must "
      "be optional: if the application does not supply it, the square "
      "root function must still compute square roots.) "
      "<br><br>"
      "These requirements apply to the modified work as a whole.  If "
      "identifiable sections of that work are not derived from the Library, "
      "and can be reasonably considered independent and separate works in "
      "themselves, then this License, and its terms, do not apply to those "
      "sections when you distribute them as separate works.  But when you "
      "distribute the same sections as part of a whole which is a work based "
      "on the Library, the distribution of the whole must be on the terms of "
      "this License, whose permissions for other licensees extend to the "
      "entire whole, and thus to each and every part regardless of who wrote "
      "it. "
      "<br><br>"
      "Thus, it is not the intent of this section to claim rights or contest "
      "your rights to work written entirely by you; rather, the intent is to "
      "exercise the right to control the distribution of derivative or "
      "collective works based on the Library. "
      "<br><br>"
      "In addition, mere aggregation of another work not based on the Library "
      "with the Library (or with a work based on the Library) on a volume of "
      "a storage or distribution medium does not bring the other work under "
      "the scope of this License. "
      "<br><br>"
      "3. You may opt to apply the terms of the ordinary GNU General Public "
      "License instead of this License to a given copy of the Library.  To do "
      "this, you must alter all the notices that refer to this License, so "
      "that they refer to the ordinary GNU General Public License, version 2, "
      "instead of to this License.  (If a newer version than version 2 of the "
      "ordinary GNU General Public License has appeared, then you can specify "
      "that version instead if you wish.)  Do not make any other change in "
      "these notices. "
      "<br><br>"
      "Once this change is made in a given copy, it is irreversible for "
      "that copy, so the ordinary GNU General Public License applies to all "
      "subsequent copies and derivative works made from that copy. "
      "<br><br>"
      "This option is useful when you wish to copy part of the code of "
      "the Library into a program that is not a library. "
      "<br><br>"
      "4. You may copy and distribute the Library (or a portion or "
      "derivative of it, under Section 2) in object code or executable form "
      "under the terms of Sections 1 and 2 above provided that you accompany "
      "it with the complete corresponding machine-readable source code, which "
      "must be distributed under the terms of Sections 1 and 2 above on a "
      "medium customarily used for software interchange. "
      "<br><br>"
      "If distribution of object code is made by offering access to copy "
      "from a designated place, then offering equivalent access to copy the "
      "source code from the same place satisfies the requirement to "
      "distribute the source code, even though third parties are not "
      "compelled to copy the source along with the object code. "
      "<br><br>"
      "5. A program that contains no derivative of any portion of the "
      "Library, but is designed to work with the Library by being compiled or "
      "linked with it, is called a \"work that uses the Library\".  Such a "
      "work, in isolation, is not a derivative work of the Library, and "
      "therefore falls outside the scope of this License. "
      "<br><br>"
      "However, linking a \"work that uses the Library\" with the Library "
      "creates an executable that is a derivative of the Library (because it "
      "contains portions of the Library), rather than a \"work that uses the "
      "library\".  The executable is therefore covered by this License. "
      "Section 6 states terms for distribution of such executables. "
      "<br><br>"
      "When a \"work that uses the Library\" uses material from a header file "
      "that is part of the Library, the object code for the work may be a "
      "derivative work of the Library even though the source code is not. "
      "Whether this is true is especially significant if the work can be "
      "linked without the Library, or if the work is itself a library.  The "
      "threshold for this to be true is not precisely defined by law. "
      "<br><br>"
      "If such an object file uses only numerical parameters, data "
      "structure layouts and accessors, and small macros and small inline "
      "functions (ten lines or less in length), then the use of the object "
      "file is unrestricted, regardless of whether it is legally a derivative "
      "work.  (Executables containing this object code plus portions of the "
      "Library will still fall under Section 6.) "
      "<br><br>"
      "Otherwise, if the work is a derivative of the Library, you may "
      "distribute the object code for the work under the terms of Section 6. "
      "Any executables containing that work also fall under Section 6, "
      "whether or not they are linked directly with the Library itself. "
      "<br><br>"
      "6. As an exception to the Sections above, you may also combine or "
      "link a \"work that uses the Library\" with the Library to produce a "
      "work containing portions of the Library, and distribute that work "
      "under terms of your choice, provided that the terms permit "
      "modification of the work for the customer's own use and reverse "
      "engineering for debugging such modifications. "
      "<br><br>"
      "You must give prominent notice with each copy of the work that the "
      "Library is used in it and that the Library and its use are covered by "
      "this License.  You must supply a copy of this License.  If the work "
      "during execution displays copyright notices, you must include the "
      "copyright notice for the Library among them, as well as a reference "
      "directing the user to the copy of this License.  Also, you must do one "
      "of these things: "
      "<br><br>"
      "a) Accompany the work with the complete corresponding "
      "machine-readable source code for the Library including whatever "
      "changes were used in the work (which must be distributed under "
      "Sections 1 and 2 above); and, if the work is an executable linked "
      "with the Library, with the complete machine-readable \"work that "
      "uses the Library\", as object code and/or source code, so that the "
      "user can modify the Library and then relink to produce a modified "
      "executable containing the modified Library.  (It is understood "
      "that the user who changes the contents of definitions files in the "
      "Library will not necessarily be able to recompile the application "
      "to use the modified definitions.) "
      "<br><br>"
      "b) Use a suitable shared library mechanism for linking with the "
      "Library.  A suitable mechanism is one that (1) uses at run time a "
      "copy of the library already present on the user's computer system, "
      "rather than copying library functions into the executable, and (2) "
      "will operate properly with a modified version of the library, if "
      "the user installs one, as long as the modified version is "
      "interface-compatible with the version that the work was made with. "
      "<br><br>"
      "c) Accompany the work with a written offer, valid for at "
      "least three years, to give the same user the materials "
      "specified in Subsection 6a, above, for a charge no more "
      "than the cost of performing this distribution. "
      "<br><br>"
      "d) If distribution of the work is made by offering access to copy "
      "from a designated place, offer equivalent access to copy the above "
      "specified materials from the same place. "
      "<br><br>"
      "e) Verify that the user has already received a copy of these "
      "materials or that you have already sent this user a copy. "
      "<br><br>"
      "For an executable, the required form of the \"work that uses the "
      "Library\" must include any data and utility programs needed for "
      "reproducing the executable from it.  However, as a special exception, "
      "the materials to be distributed need not include anything that is "
      "normally distributed (in either source or binary form) with the major "
      "components (compiler, kernel, and so on) of the operating system on "
      "which the executable runs, unless that component itself accompanies "
      "the executable. "
      "<br><br>"
      "It may happen that this requirement contradicts the license "
      "restrictions of other proprietary libraries that do not normally "
      "accompany the operating system.  Such a contradiction means you cannot "
      "use both them and the Library together in an executable that you "
      "distribute. "
      "<br><br>"
      "7. You may place library facilities that are a work based on the "
      "Library side-by-side in a single library together with other library "
      "facilities not covered by this License, and distribute such a combined "
      "library, provided that the separate distribution of the work based on "
      "the Library and of the other library facilities is otherwise "
      "permitted, and provided that you do these two things: "
      "<br><br>"
      "a) Accompany the combined library with a copy of the same work "
      "based on the Library, uncombined with any other library "
      "facilities.  This must be distributed under the terms of the "
      "Sections above. "
      "<br><br>"
      "b) Give prominent notice with the combined library of the fact "
      "that part of it is a work based on the Library, and explaining "
      "where to find the accompanying uncombined form of the same work. "
      "<br><br>"
      "8. You may not copy, modify, sublicense, link with, or distribute "
      "the Library except as expressly provided under this License.  Any "
      "attempt otherwise to copy, modify, sublicense, link with, or "
      "distribute the Library is void, and will automatically terminate your "
      "rights under this License.  However, parties who have received copies, "
      "or rights, from you under this License will not have their licenses "
      "terminated so long as such parties remain in full compliance. "
      "<br><br>"
      "9. You are not required to accept this License, since you have not "
      "signed it.  However, nothing else grants you permission to modify or "
      "distribute the Library or its derivative works.  These actions are "
      "prohibited by law if you do not accept this License.  Therefore, by "
      "modifying or distributing the Library (or any work based on the "
      "Library), you indicate your acceptance of this License to do so, and "
      "all its terms and conditions for copying, distributing or modifying "
      "the Library or works based on it. "
      "<br><br>"
      "10. Each time you redistribute the Library (or any work based on the "
      "Library), the recipient automatically receives a license from the "
      "original licensor to copy, distribute, link with or modify the Library "
      "subject to these terms and conditions.  You may not impose any further "
      "restrictions on the recipients' exercise of the rights granted herein. "
      "You are not responsible for enforcing compliance by third parties with "
      "this License. "
      "<br><br>"
      "11. If, as a consequence of a court judgment or allegation of patent "
      "infringement or for any other reason (not limited to patent issues), "
      "conditions are imposed on you (whether by court order, agreement or "
      "otherwise) that contradict the conditions of this License, they do not "
      "excuse you from the conditions of this License.  If you cannot "
      "distribute so as to satisfy simultaneously your obligations under this "
      "License and any other pertinent obligations, then as a consequence you "
      "may not distribute the Library at all.  For example, if a patent "
      "license would not permit royalty-free redistribution of the Library by "
      "all those who receive copies directly or indirectly through you, then "
      "the only way you could satisfy both it and this License would be to "
      "refrain entirely from distribution of the Library. "
      "<br><br>"
      "If any portion of this section is held invalid or unenforceable under any "
      "particular circumstance, the balance of the section is intended to apply, "
      "and the section as a whole is intended to apply in other circumstances. "
      "<br><br>"
      "It is not the purpose of this section to induce you to infringe any "
      "patents or other property right claims or to contest validity of any "
      "such claims; this section has the sole purpose of protecting the "
      "integrity of the free software distribution system which is "
      "implemented by public license practices.  Many people have made "
      "generous contributions to the wide range of software distributed "
      "through that system in reliance on consistent application of that "
      "system; it is up to the author/donor to decide if he or she is willing "
      "to distribute software through any other system and a licensee cannot "
      "impose that choice. "
      "<br><br>"
      "This section is intended to make thoroughly clear what is believed to "
      "be a consequence of the rest of this License. "
      "<br><br>"
      "12. If the distribution and/or use of the Library is restricted in "
      "certain countries either by patents or by copyrighted interfaces, the "
      "original copyright holder who places the Library under this License may add "
      "an explicit geographical distribution limitation excluding those countries, "
      "so that distribution is permitted only in or among countries not thus "
      "excluded.  In such case, this License incorporates the limitation as if "
      "written in the body of this License. "
      "<br><br>"
      "13. The Free Software Foundation may publish revised and/or new "
      "versions of the Lesser General Public License from time to time. "
      "Such new versions will be similar in spirit to the present version, "
      "but may differ in detail to address new problems or concerns. "
      "<br><br>"
      "Each version is given a distinguishing version number.  If the Library "
      "specifies a version number of this License which applies to it and "
      "\"any later version\", you have the option of following the terms and "
      "conditions either of that version or of any later version published by "
      "the Free Software Foundation.  If the Library does not specify a "
      "license version number, you may choose any version ever published by "
      "the Free Software Foundation. "
      "<br><br>"
      "14. If you wish to incorporate parts of the Library into other free "
      "programs whose distribution conditions are incompatible with these, "
      "write to the author to ask for permission.  For software which is "
      "copyrighted by the Free Software Foundation, write to the Free "
      "Software Foundation; we sometimes make exceptions for this.  Our "
      "decision will be guided by the two goals of preserving the free status "
      "of all derivatives of our free software and of promoting the sharing "
      "and reuse of software generally. "
      "<br><br>"
      "NO WARRANTY "
      "<br><br>"
      "15. BECAUSE THE LIBRARY IS LICENSED FREE OF CHARGE, THERE IS NO "
      "WARRANTY FOR THE LIBRARY, TO THE EXTENT PERMITTED BY APPLICABLE LAW. "
      "EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR "
      "OTHER PARTIES PROVIDE THE LIBRARY \"AS IS\" WITHOUT WARRANTY OF ANY "
      "KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE "
      "IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR "
      "PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE "
      "LIBRARY IS WITH YOU.  SHOULD THE LIBRARY PROVE DEFECTIVE, YOU ASSUME "
      "THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION. "
      "<br><br>"
      "16. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN "
      "WRITING WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY "
      "AND/OR REDISTRIBUTE THE LIBRARY AS PERMITTED ABOVE, BE LIABLE TO YOU "
      "FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR "
      "CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE "
      "LIBRARY (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING "
      "RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A "
      "FAILURE OF THE LIBRARY TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF "
      "SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH "
      "DAMAGES. "
      "<br><br>"
      "END OF TERMS AND CONDITIONS ";
   
   QTextEdit* pNoticeEdit = new QTextEdit(this);
   pNoticeEdit->setLineWrapMode(QTextEdit::WidgetWidth);
   pNoticeEdit->setReadOnly(true);
   pNoticeEdit->setHtml(strNotice);

   QPalette pltNotice = pNoticeEdit->palette();
   pltNotice.setColor(QPalette::Base, Qt::lightGray);
   pNoticeEdit->setPalette(pltNotice);

   QTabWidget* pLicenseTabBox = new QTabWidget(this);
   pLicenseTabBox->addTab(pNoticeEdit, APP_NAME);

   for (unsigned int itemCount = 0;
        itemCount < brandings.size();
        ++itemCount)
   {
      string license = brandings[itemCount].getLicense();
      if (license.empty())
      {
         continue;
      }
      QTextEdit *pEdit = new QTextEdit(this);
      pEdit->setLineWrapMode(QTextEdit::WidgetWidth);
      pEdit->setReadOnly(true);
      pEdit->setHtml(QString::fromStdString(license));
      QPalette plt = pEdit->palette();
      plt.setColor(QPalette::Base, Qt::lightGray);
      pEdit->setPalette(plt);
      pLicenseTabBox->addTab(pEdit, QString::fromStdString(brandings[itemCount].getTitle()));
   }


   LabeledSection* pCopyrightSection = new LabeledSection(pLicenseTabBox, APP_NAME " and Plug-In Suite Licenses");
   pAboutGroup->addSection(pCopyrightSection, 1000);

   // Create the Special license box
   QTabWidget* pSpecialLicensesTabBox = new QTabWidget(this);

   map<string, string> dependencyCopyrights;
   vector<PlugInDescriptor*> pluginDescriptors = Service<PlugInManagerServices>()->getPlugInDescriptors();
   for(vector<PlugInDescriptor*>::iterator pluginDescriptor = pluginDescriptors.begin();
      pluginDescriptor != pluginDescriptors.end(); ++pluginDescriptor)
   {
      if(*pluginDescriptor != NULL)
      {
         map<string, string> tmp = (*pluginDescriptor)->getDependencyCopyright();
         for(map<string, string>::iterator tmpIt = tmp.begin(); tmpIt != tmp.end(); ++tmpIt)
         {
            map<string, string>::iterator current = dependencyCopyrights.find(tmpIt->first);
            VERIFYNR_MSG(current == dependencyCopyrights.end() || current->second == tmpIt->second,
               "A duplicate dependency copyright message name was found but the copyright message "
               "did not match the existing message.");
            dependencyCopyrights[tmpIt->first] = tmpIt->second;
         }
      }
   }
   for(map<string, string>::const_iterator dependencyCopyright = dependencyCopyrights.begin();
      dependencyCopyright != dependencyCopyrights.end(); ++dependencyCopyright)
   {
      QTextEdit *pEdit = new QTextEdit(this);
      pEdit->setLineWrapMode(QTextEdit::WidgetWidth);
      pEdit->setReadOnly(true);
      pEdit->setHtml(QString::fromStdString(dependencyCopyright->second));
      QPalette plt = pEdit->palette();
      plt.setColor(QPalette::Base, Qt::lightGray);
      pEdit->setPalette(plt);
      pSpecialLicensesTabBox->addTab(pEdit, QString::fromStdString(dependencyCopyright->first));
   }

   LabeledSection* pLicenseSection = new LabeledSection(pSpecialLicensesTabBox, "Third-Party Licenses");
   pAboutGroup->addSection(pLicenseSection, 1000);

   pAboutGroup->addStretch(1);
   pAboutGroup->collapseSection(pLicenseSection);
   pAboutGroup->collapseSection(pCopyrightSection);

   // Create the OK button
   QPushButton* pOk = new QPushButton("&OK", this);
   connect(pOk, SIGNAL(clicked()), this, SLOT(accept()));

   // Dialog layout
   QVBoxLayout* pLayout = new QVBoxLayout(this);
   pLayout->setMargin(10);
   pLayout->setSpacing(10);
   pLayout->addWidget(pAboutGroup, 10);
   pLayout->addWidget(pOk, 0, Qt::AlignRight);

   // Initialization
   setWindowTitle(QString("About %1").arg(APP_NAME));
   setModal(true);
   resize(400, 425);
}

AboutDlg::~AboutDlg()
{
}