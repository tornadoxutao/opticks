/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef CONFIGURATIONSETTINGSIMP_H
#define CONFIGURATIONSETTINGSIMP_H

#include "ConfigurationSettings.h"
#include "ObjectResource.h"
#include "SubjectImp.h"
#include "TypesFile.h"

#include <map>
#include <string>
#include <vector>

class DataDescriptor;
class DateTime;
class DynamicObject;
struct MruFile;
class XMLWriter;

/**
 *  Contains settings specific to the application, user, and plug-ins.
 *
 *  This is a singleton class.  Only one instance of this class exists at
 *  a given time.  Use the instance() method to get a reference to the class.
 *  The class provides the additional capability over the parent interface
 *  class to reset the settings to their default values, and to quickly
 *  construct plug-in settings for the generic importer for Studio objects.
 *
 *  @see    ConfigurationSettings
 */
class ConfigurationSettingsImp : public ConfigurationSettings, public SubjectImp
{
public:
   SETTING(ReleaseType, General, ReleaseType, RT_NORMAL); 
   /**
    *  Returns the instance of this singleton class.
    *
    *  The instance() method controls the instantiation of this class  
    *  and returns a reference to the singleton instance.  If the class 
    *  has not been instantiated, it creates the instance, stores
    *  it internally, and then returns a reference to the new 
    *  reference.
    *
    *  @return  This method returns the singleton class instance.
    */
   static ConfigurationSettingsImp* instance();
   static void destroy();

   std::string getHome() const;
   std::string getCreator() const;
   std::string getProduct() const;
   std::string getVersion() const;
   std::string getBuildRevision() const;
   std::string getUserName() const;
   std::string getOperatingSystemName() const;
   std::string getArchitectureName() const;

   const DateTime* getReleaseDate() const;
   bool isProductionRelease() const;
   ReleaseType getReleaseType() const;

   bool isInitialized();
   const char* getInitializationErrorMsg();

   bool setSetting(const std::string &key, const DataVariant &var, bool setIfSame = false);
   const DataVariant &getSetting(const std::string& key) const;
   bool isUserSetting(const std::string& key) const;
   void deleteUserSetting(const std::string& key);
   void deleteSessionSetting(const std::string& key);
   bool setSessionSetting(const std::string& key, const DataVariant& var);
   void copySetting(const std::string& key, DynamicObject* pObject) const;

   void setMruFiles(const std::vector<MruFile>& mruFiles);
   const std::vector<MruFile>& getMruFiles() const;

   bool serializeAsDefaults(const Filename* pFilename, const DynamicObject* pObject) const;
   DynamicObject* deserialize(const Filename* pFilename) const;

   void updateProductionStatus();

   const std::string& getObjectType() const
   {
      static std::string type = "ConfigurationSettingsImp";
      return type;
   }

   bool isKindOf(const std::string& className) const
   {
      if ((className == getObjectType()) || (className == "ConfigurationSettings"))
      {
         return true;
      }
      return SubjectImp::isKindOf(className);
   }

   SUBJECTADAPTER_METHODS(SubjectImp);

protected:
   std::string getUserSettingsFile(bool createDir = false) const;

   bool serialize() const;
   bool deserialize();
   void applicationClosed(Subject& subject, const std::string& signal, const boost::any &args);

   /**
    *  Find the application home directory
    *
    *  @return The home directory for the application
    */
   static std::string locateApplicationHome();

   /**
    *  Constructor which can NOT be called outside this class.
    *
    *  The default constructor is protected.  The allows the instance()
    *  method to control the number of instances of this class.  To
    *  instantiate this class the ConfigurationSettings::instance() 
    *  method must be called.
    */
   ConfigurationSettingsImp();

   /**
    *  Destructor which can not be invoked outside this class.
    *
    *  This destructor deletes all dynamic memory associated with the
    *  class.
    */
   virtual ~ConfigurationSettingsImp();

   /**
    * Serializes the settings to the given file.
    *
    * @param filename
    *        the file that the settings should be saved to.
    *
    * @param pSettings
    *        the settings to serialize to the file.
    *
    * @param saveMru
    *        if true, the mruFile will be serialized to the file as well.
    *
    * @return True on a successful operation, False otherwise.
    */
   bool serializeSettings(const std::string& filename, const DynamicObject* pSettings, bool saveMru) const;

private:
   static ConfigurationSettingsImp* spInstance;
   static bool mDestroyed;

   std::string mCreator;
   std::string mProduct;
   std::string mVersion;
   DateTime* mpReleaseDate;
   bool mProductionRelease;
   ReleaseType mReleaseType;

   FactoryResource<DynamicObject> mpUserSettings;
   FactoryResource<DynamicObject> mpSessionSettings;
   FactoryResource<DynamicObject> mpDefaultSettings;

   mutable std::vector<MruFile> mMruFiles;
   mutable bool mNeedToLoadMruFiles;

   std::string mHomePath;

   std::string mInitializationErrorMsg;
   bool mIsInitialized;
};

#endif