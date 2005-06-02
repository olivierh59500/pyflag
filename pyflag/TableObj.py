# ******************************************************
# Copyright 2004: Commonwealth of Australia.
#
# Developed by the Computer Network Vulnerability Team,
# Information Security Group.
# Department of Defence.
#
# Michael Cohen <scudette@users.sourceforge.net>
#
# ******************************************************
#  Version: FLAG $Version: Q Branch Database 2.0.1 (Based on Pyflag 0.76) Date: Thu Apr 28 23:08:32 EST 2005$
# ******************************************************
#
# * This program is free software; you can redistribute it and/or
# * modify it under the terms of the GNU General Public License
# * as published by the Free Software Foundation; either version 2
# * of the License, or (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# ******************************************************

""" This module manages access to tables.

It provides simple reports for adding, deleting, and editing records
within tables.
"""

import pyflag.Reports as Reports
import pyflag.FlagFramework as FlagFramework
import pyflag.conf
config=pyflag.conf.ConfObject()
import pyflag.DB as DB
import pyflag.TypeCheck as TypeCheck
import plugins.Global as Global

class ConstraintError(Exception):
    """ This exception is thrown when a contraint failed when adding or updating a row in the db.
    """
    def __init__(self,result):
        self.result=result

class TableObj:
    """ An abstract object representing a table in the database """
    table = ""
    key = 'id'
    ## This maps columns to names. Columns that are not listed here
    ## are ignored:
    columns = ()
    _column_keys = ()
    _column_names = ()
    ## This maps columns to functions that are used to generate
    ## specific form elements. If there is no mapping, we
    ## automatically produce textfield. The prototype is:
    
    ##    def function(description, name, results)
    
    ## Where name is the name of the query parameter that will be
    ## generated. query is the query string, and results is the ui to
    ## draw on.
    input_types={}
    ## These are functions run over each column named. They can stop
    ## the operation by throwing an exception or merely return a new
    ## value to be added. The prototype is:

    ## def function(fieldname,proposed_value,id)

    add_constraints = {}
    edit_constraints = {}
    display_actions = {}
    delete_actions = {}
    
    def __init__(self,dbh=None,id=None):
        if dbh:
            self.dbh=dbh
        else:
            self.dbh=DB.DBO(None)
        self.id=id
        self._column_keys= [ self.columns[i] for i in range(0,len(self.columns)) if not i % 2 ]
        self._column_names = [ self.columns[i] for i in range(0,len(self.columns)) if i % 2 ]

    def _make_column_sql(self):
        return ','.join([ "%s as %r" % (self._column_keys[i],self._column_names[i]) for i in range(len(self._column_keys)) ])

    def __getitem__(self,id):
        """ Emulates a table accessor.

        id is the key value which will be retrieved. We return a row record.
        """
        self.dbh.execute("select %s from %s where %s=%r",(self._make_column_sql(),self.table,self.key,id))
        return self.dbh.fetch()

    def edit(self,query,results):
        """ Updates the row with id given in query[self.table.key] to the values in query """
        ##First we check all the fields through their constraints:
        for k,v in self.edit_constraints.items():
            if query.has_key(k):
                replacement=v(self,k,query[k],id=query[self.key],query=query,result=results)
            else:
                replacement=v(self,k,None,id=None,query=query,result=results)

            if(replacement):
                del query[k]
                query[k]=replacement

        ## Make up the SQL Statement. Note that if query is missing a
        ## parameter which we need, we simply do not update it.
        tmp = []
        for k in self._column_keys:
            try:
                tmp.append("%s=%r" % (k,query[k]))
            except KeyError:
                pass
            
        self.dbh.execute("UPDATE %s set %s where %s=%r ",(self.table,
                         ','.join(tmp),
                         self.key, query[self.key]))
        
        results.para("The following is the new record:")
        self.show(query[self.key],results)

    def add(self,query,results):
        """ Adds a row given in query[self.table.key] to the table """
        ##First we check all the fields through their constraints:
        for k,v in self.add_constraints.items():
            if query.has_key(k):
                replacement=v(self,k,query[k],id=None,query=query,result=results)
            else:
                replacement=v(self,k,None,id=None,query=query,result=results)

            if(replacement):
                del query[k]
                query[k]=replacement
        
        ## Work out which columns need to be changed
        result=[]
        for k in self._column_keys:
            try:
                result.append("%s=%r" % (k,query[k]))
            except KeyError:
                pass
                
        self.dbh.execute("insert into %s set %s ",(self.table,
                         ",".join(result),
                         ))
        
    def get_name(self,col):
        """ Returns the name description for the column specified """
        return self._column_names[self._column_keys.index(col)]
    
    def form(self,columns,query,results,defaults=None):
        """ Draws a form.

        @arg columns: A list of the columns specified
        @arg query: The global query object
        @arg results: The UI to draw in
        @arg defaults: A dictionary of defaults to assign into query
        """
        results.start_table()
        for k,v in zip(self._column_keys,self._column_names):
            try:
                ## If there is no input from the user - override the input from the database:
                if defaults and not query.has_key(k):
                    query[k]=defaults[k]

                ## Here we check if there are special form functions to be executed:
                try:
                    if not query.has_key(k):
                        query[k]=''
                    
                    self.form_actions[k](description=v,ui=results,variable=k)
                # If not we display a text field
                except KeyError:
                    results.textfield(v,k,size=40)
            except KeyError:
                pass

    def add_form(self,query,results):
        """ Generates a form to add a new record in the current table """
        self.form( self._column_keys ,query,results)

    def edit_form(self,query,results):
        """ Generates an editing form for the current table """
        id=query[self.key]
        
        self.dbh.execute("select * from %s where %s=%r",(self.table,self.key,id))
        row=self.dbh.fetch()
        self.form(self._column_keys,query,results,row)

    def delete(self,id,result,commit=False):
        """ This deletes the row with specified id from the table.

        Note that self.delete_actions is run to ensure that database consistancy is maintained. This method actually does the deletion, where show_delete describes what will be deleted but does not actually delete stuff (it uses self.show_deletes). It is up to derived classes to ensure that the two attributes return consistant results.

        If commit is True we really go ahead and delete, otherwise we just write on result what we are trying to do.
        """
        tmp = result.__class__(result)
        tmp.text("Will try to remove %s %s=%s. This record is shown here:" % (self.table,self.key,id),font='bold',color='red')
        result.row(tmp)
        self.show(id,result)
        
        for k,v in zip(self._column_keys,self._column_names):
            if self.delete_actions.has_key(k):
                value = row[k]
                self.delete_actions[k](description=v, variable=k, value=value, ui=result, row=row, id=id)

        if commit:
            self.dbh.execute("delete from %s where %s=%r",
                             ( self.table,self.key,id))

    def show(self,id,result):
        self.dbh.execute("select %s from %s where %s=%r",
                         ( ','.join(self._column_keys),
                           self.table,self.key,id))

        row=self.dbh.fetch()
        if not row:
            tmp=result.__class__(result)
            tmp.text("Error: Record %s not found" % id,color='red')
            result.row(tmp)
            return
        
        result.start_table()
        for k,v in zip(self._column_keys,self._column_names):
            ## If there are any specific display functions we let them
            ## do it here...
            if self.display_actions.has_key(k):
                value = row[k]
                self.display_actions[k](description=v, variable=k, value=value, ui=result, row=row, id=id)
            else:
                try:
                    tmp = result.__class__(result)
                    tmp.text(row[k],color='red')
                    result.row(v,tmp)
                except KeyError:
                    pass
