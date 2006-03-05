## pyFLAG utility script designed to run in a cron to allow automated moving/copying and pyflag processing of data
## gregsfdev@users.sourceforge.net

## This is really new, so it is still pretty rough.

###TODO:
######## Create more flag feeders to handle dd images etc. (will have to change cmd line options to do this too).


"""This script copies data files from a (possibly remote) location, and ensures they haven't been corrupted by running md5sum on both ends.  The files are then processed through pyflag using pyflash and the logs copied to a (possibly remote) location.  The source files need to be uniquely named, otherwise they will overwrite each other.  The script creates a lock file so that it can be run safely in a cron without trashing itself.  Files are copied with scp - it is assumed that private/public keys have been setup to allow this transfer.

"""

from optparse import OptionParser
import os, shutil, glob

class DataHandler:
    def create(self,path):
        if (path.find("@") >=0 ):
            data_source=Remote(path)                
        else:
            data_source=Local(path)
        return data_source

class Date:
    def __init__(self, format):
        output = os.popen("date +" + format)
        self.current = output.readlines()[0].strip()

class Remote:
    """This class is for remote data - i.e. data not available through the local filesystem.  We will use ssh to interact with it (assumes authorized keys have been set up)"""
    def __init__(self,path):
        #this is a remote file(s) that we need to scp
        self.user=path.split("@")[0]
        self.host=path.split(":")[0].split("@")[1]
        self.path=path.split(":")[1]
        self.dirname=os.path.dirname(self.path)
        self.ssh_prefix="ssh " + self.user + "@" + self.host
            
    def create_file_list(self,path):

        files=os.path.basename(path)
        dirname=os.path.dirname(path)

        #Look for files whose status hasn't changed for 5 minutes.  This means we won't try to copy files that are still being written to.
        if files=="*":
            #If the user has specified all files in the dir, we need to use a slightly different find command
            find_cmd=self.ssh_prefix + " \'find " + dirname + " -print -cmin +5\'"
        else:
            find_cmd=self.ssh_prefix + " \'find " + dirname + " -name \"" + files +"\" -cmin +5\'"
            log.write(find_cmd)

        try:
            filelist = os.popen(find_cmd)
        except:
            log.write("error executing: " + find_cmd)
            
        log.write("List of remote files:")
        self.list=[]
        for line in filelist:
            log.write("--  " + line)
            self.list.append(line.strip())

    def md5sum(self, filename):
        #get remote md5sum
        output = os.popen(self.ssh_prefix + " md5sum " + filename)
        self.md5 = output.readlines()[0].split()[0].strip()
        log.write("remote md5 of " + filename + " is " + self.md5)

    def moveandcheck(self, dest):

        self.create_file_list(self.path)
        files_copied_successfully=0
        errors=0
        
        for filename in self.list:
            try:
                
                self.md5sum(filename)

                #copy over file
                local_file=os.path.basename(filename)

                output = os.popen("scp " + self.user + "@" + self.host + ":" + filename + " " + os.path.join(dest.path,local_file) )
                log.write("scp " + self.user + "@" + self.host + ":" + filename + " " + os.path.join(dest.path,local_file) )

                #do this to ensure the process gets completed
                for line in output:
                    print line
                log.write(filename + " successfully transferred to " + os.path.join(dest.path,local_file))

                #get local md5sum
                output = os.popen("md5sum " + os.path.join(dest.path,local_file) )
                localmd5 = output.readlines()[0].split()[0]
                log.write("local md5 of " + local_file + " is " + localmd5)  

                #compare md5sums and delete remote file
                if (self.md5 == localmd5.strip()):
                    log.write("md5 for " + filename + " is correct")
                    ##Delete file - don't enable till this works properly
                    ##blah
                    if (options.removedata):
                        log.write(self.ssh_prefix + " rm " + filename)
                        os.popen(self.ssh_prefix + " rm " + filename)
                        log.write("Removed file: %s from the server" % filename)
                    else:
                        log.write("Leaving file: %s on the server" % filename)
                    files_copied_successfully +=1
                else:
                    log.write("error: md5s did not match for " + filename + " did not remove from server")
                    errors +=1

            except KeyboardInterrupt:
                log.write("notice: user has terminated execution, exiting")
                sys.exit()

            except Exception,args:
                log.write("error occurred copying file " + filename + " continuing processing")
                log.write("details: " + str(args))
        return (files_copied_successfully,errors)

class Local:
    def __init__(self,path):
        self.path=path
        self.dirname=os.path.dirname(self.path)
        self.list=glob.glob(self.path)
        
        if (os.path.isdir(self.path)):
            try:
                os.stat(self.path)
            except OSError:
                #try to make dir
                print "Creating local directory :%s" % self.path
                os.makedirs(self.path)
        

    def move(self,dest):
        if (dest.__class__==Remote):
            #Move this local data somewhere remotely, don't do any checking
            print("scp " + self.path + " " + dest.user + "@" + dest.host + ":" + dest.path)
            os.system("scp " + self.path + " " + dest.user + "@" + dest.host + ":" + dest.path)
            print("Removing file " + self.path)
            os.remove(self.path)
        else:
            print("Moving " + self.path + " to " + dest.path)
            shutil.move(self.path,dest.path)

    def moveandcheck(self,dest):
        #No point in doing md5 check, as we are just moving the OS reference to the file.  Maybe use mv instead?
        files_copied_successfully=0
        errors=0
        for filename in self.list:
            try:
                print "Moving %s to %s" % (filename,dest.path)
                shutil.move(filename, dest.path)
                files_copied_successfully +=1
            except KeyboardInterrupt:
                log.write("notice: user has terminated execution, exiting")
                sys.exit()

            except Exception,args:
                log.write("error occurred processing file " + filename + " continuing processing")
                log.write("details: " + str(args))
                errors +=1
        return (files_copied_successfully,errors)
                
class CaseDateDir(Local):
    """This is a class that creates a dirctory of casename_date, appended to the specified path."""
    
    def __init__(self,path):
        Local.__init__(self,path)
        self.path=os.path.join(self.path , options.casename + "_" + date.current)
        self.dirname=os.path.dirname(self.path)
        try:
            os.stat(self.path)
        except OSError:
            #try to make dir
            print "Creating local directory :%s" % self.path
            os.makedirs(self.path)
            
    def movedir(self,dest):
        #Moves the directory
        if (dest.__class__==Remote):
            #Move this local data somewhere remotely, don't do any checking
            print("scp -r " + self.dirname + " " + dest.user + "@" + dest.host + ":" + dest.path)
            os.system("scp -r " + self.dirname + " " + dest.user + "@" + dest.host + ":" + dest.path)
            print("Removing directory " + self.dirname)
            shutil.rmtree(self.dirname)
        elif(self.__class__==Logfile):
            print("Moving " + self.dirname + " to " + dest.path)
            os.system("mv " + self.dirname + " " + dest.path)
        elif (dest.__class__==CaseDateDir):
            print("Moving " + self.path + " to " + dest.dirname)
            os.system("mv " + self.path + " " + dest.dirname)
        elif (dest.__class__==Local):
            print("Moving " + self.path + " to " + dest.path)
            os.system("mv " + self.path + " " + dest.path)

class Logfile(CaseDateDir):
    """This is a class for creating a local logfile used by this script"""
    def __init__(self,sink_str):
        CaseDateDir.__init__(self,sink_str)
        self.path=os.path.join(self.path, options.casename + "_" + date.current + "_" + "copylog.txt")
        self.dirname=os.path.dirname(self.path)
        try:
            self.handle = open(self.path, "w")
        except IOError:
            print "Error opening file %s" % self.path
    def write(self,msg):
        self.handle.writelines(date.current + ": " + msg + "\n")
        print(date.current + ": " + msg)
    def closeloghandle(self):
        self.handle.close()
    

class FlagFeeder:
    """This class feeds data through pyflag using pyflash"""
    def pcap(self,source,pyflashconf,casename,error_log_dir,handler):
        errors=0
        #For each pcap in the directory given, run it through flag
        for files in glob.glob(os.path.join(source.path,"*.pcap")):
            local_file_for_processing = os.path.basename(files).replace(".pcap","")
            pyflash_log_file=os.path.join(os.path.dirname(log.path), casename + "_" + date.current + "_" + local_file_for_processing + ".pyflag_log")
            log.write("Pyflash logfile: " + pyflash_log_file)

            #log.write("pyflash -c " + pyflashconf + " -p case:" + casename + local_file_for_processing + ",iosource:" + casename + local_file_for_processing + ",iofilename:" + files + ",mountpoint:" + casename + " &> " + pyflash_log_file)            
            log.write("pyflash -c " + pyflashconf + " -p case:" + casename + ",iosource:" + local_file_for_processing + ",iofilename:" + files + ",mountpoint:" + local_file_for_processing + " &> " + pyflash_log_file)
            os.system("pyflash -c " + pyflashconf + " -p case:" + casename + ",iosource:" + local_file_for_processing + ",iofilename:" + files + ",mountpoint:" + local_file_for_processing + " &> " + pyflash_log_file)

            if self.success(pyflash_log_file):
                pass
            else:
                #Pyflash run did not complete sucessfully, copy the pyflash log to the location specified by --errorlogdir
                log.write("Errors recorded in " + pyflash_log_file + " moved to: " + error_log_dir)
                log_file=Local(pyflash_log_file)
                log_dest=handler.create(error_log_dir)
                log_file.move(log_dest)
                errors +=1
        return errors

    def success(self,pyflash_log_file):
        try:
            log_file = open(pyflash_log_file, "r")
        except IOError:
            print "Error opening file %s" % pyflash_log_file
        for line in log_file:
            if (line.find("Execution of Load Data.ScanFS successful") >=0 ):
                log.write("Pyflash execution successful")
                return 1
        log.write("Pyflash execution did not complete successfully")
        return 0            

class LockFile:
    """Creates a lock file in the specified path to make sure we don't trash a previous instance of the program."""
    def __init__(self,newlockfile):
        self.path=newlockfile
        if (os.path.exists(self.path)):
            raise Exception, "Lock file %s exists!  Maybe the previous cronjob has not finished??" % self.path
        else:
            try:
                self.handle = open(self.path, "w")
            except IOError:
                print "Error creating lock file %s" % self.path
            self.handle.writelines(str(os.getpid()) + "\n")
            self.handle.close()
    def __del__(self):
        #Python will cleanup the logfile for us, since it is in the destructor
        os.remove(self.path)

        

#####################
##### MAIN
#####################
##The main method provided below is an example - users can comment out the bits they don't need, or rewrite it.
####

parser = OptionParser(version="%prog 1.0")

parser.add_option("-s", "--src",type="string", dest="src", help="The source of the file(s) to be processed.  This can be local or in scp syntax including wildcards, e.g. /file/path/*.dd user@host:/path/to/file.*  Source file names must be unique, or they may overwrite each other."),
parser.add_option("-c", "--pyflash-file",type="string", dest="pyflashconf", help="The pyflash config file to use (must be local file)"),
parser.add_option("-l", "--logdir",type="string", dest="logdir", help="The directory to put the pyflash log files in.  This can be local or in scp syntax, e.g. /my/logdir user@host:/path/to/dir"),
parser.add_option("-e", "--errorlogdir",type="string", dest="errorlogdir", help="The directory to put the pyflash log files in if the copying or pyflash script doesn't complete successfully.  This can be local or in scp syntax, e.g. /my/error/logdir user@host:/path/to/dir"),
parser.add_option("-t", "--tempdir",type="string", dest="tempdir", help="Temporary directory to store log files as they are being written (must be local)"),
parser.add_option("-d", "--holdingdir",type="string", dest="holdingdir", help="Temporary directory to store the data for processing (must be local, will be moved from --src)"),
parser.add_option("-n", "--casename",type="string", dest="casename", help="The casename to use in pyFLAG for these files"),
parser.add_option("-f", "--lockfile",type="string", dest="lockfilepath", help="The lockfile for the program (must be local)"),
parser.add_option("-r", "--removepostprocess",action="store_true",dest="removedata",help="If this flag is set, data will be removed from source after it has been successfully copied  holdingdir.  Only necessary if source is remote - if src is local, the data is moved (not copied) to holdingdir regardless of this flag."),
parser.add_option("-b", "--backupdir",type="string", dest="backupdir", help="The local directory to backup all data (excepts logs) to after processing is finished (this is the only copy of the data that will remain)")

(options, args) = parser.parse_args()

date=Date("%Y%m%d_%H%M")

#Use this handler when we don't know what sort of object (local or remote) we want
handler=DataHandler()

#Create our log object (must be called "log") as it is used globally to save passing as argument to everywhere.
log=Logfile(options.tempdir)
log.write("logfile created at " + log.path)


try:
    #Make sure we don't trash a previous cron that is still processing.
    lock=LockFile(options.lockfilepath)
    log.write("lock file created at: %s" % lock.path)
    
    data_source=handler.create(options.src)

    local_data_store=CaseDateDir(options.holdingdir)
    
    #Get data here so we can process it.
    (num_files,errors)=data_source.moveandcheck(local_data_store)


    if (num_files > 0):
        #we copied at least one file, so let's work on it
        log.write("Successfully copied: %s files" % num_files)

        #FLAG Processing
        log.write("########### File copying finished, starting pyflag processing #########")
        flag_feeder=FlagFeeder()
        flag_err=flag_feeder.pcap(local_data_store,options.pyflashconf,options.casename,options.errorlogdir,handler)
        log.write("Flag processing completed with errors in %s files" % flag_err)

        #Backing up data
        log.write("Backing up data to " + options.backupdir)
        backup_dir=Local(options.backupdir)
        local_data_store.movedir(backup_dir)
        log.write("Directory " + local_data_store.path + " moved to backup location: " + backup_dir.path)

        if ((errors > 0) or (flag_err > 0)):
            log.write("%s error(s) were encountered during copying.  %s error(s) were encountered during flag processing. See logs in %s" % (errors,flag_err,options.errorlogdir))
            log_dest=handler.create(options.errorlogdir)
        else:
            #Exited normally! Writing logs
            log_dest=handler.create(options.logdir)
    else:
        #Nothing copied, but this is not necessarily bad.
        log.write("0 file(s) were copied.")
        log_dest=handler.create(options.logdir)
            
except Exception,args:
    log.write(str(args))
    log_dest=handler.create(options.errorlogdir)

log.write("Moving logs to: " + log_dest.path)
log.closeloghandle()
log.movedir(log_dest)



