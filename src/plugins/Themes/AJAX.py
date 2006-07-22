""" An AJAX based theme """
import pyflag.conf
config=pyflag.conf.ConfObject()
import pyflag.FlagFramework as FlagFramework
import pyflag.Registry as Registry
import pyflag.Theme as Theme
import pyflag.DB as DB
from plugins.Themes.Menus import Menu

class AJAX(Menu):
    header='''<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
        <html>
        <head>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
        <title>%s</title>
        <link rel="stylesheet" type="text/css" href="images/ajax_ui.css" />
        <script type="text/javascript">
        var djConfig = { isDebug: true, baseScriptUri: "javascript/" };
        </script>
        <script type="text/javascript" src="/javascript/dojo.js"></script>
 
        <script type="text/javascript">
        dojo.require("dojo.widget.ComboBox");
        dojo.require("dojo.widget.SplitContainer");
	dojo.require("dojo.widget.ContentPane");
        dojo.require("dojo.widget.TabContainer");
	dojo.require("dojo.widget.LinkPane");
	dojo.require("dojo.widget.LayoutContainer");
        dojo.require("dojo.widget.Tree");
        dojo.require("dojo.widget.TreeSelector");
        dojo.require("dojo.widget.TreeLoadingController");
        dojo.require("dojo.widget.Menu2");
	dojo.require("dojo.widget.Button");
        dojo.require("dojo.widget.PyFlagTable");
        dojo.require("dojo.widget.Dialog");
        dojo.require("dojo.widget.Toolbar");
        dojo.require("dojo.uri.dojoUri");
        dojo.require("dojo.collections.Stack");

        </script>
        <script type="text/javascript" src="/javascript/ajax_misc.js"></script>
        <style type="text/css" src="/images/ajax_ui.css"></style>
        
        </head>
        <body link=blue vlink=blue bgcolor="#FFFFFF">
        
        <div dojoType="dialog" id="FilterDialog" bgColor="white" bgOpacity="0.5" toggle="fade" toggleDuration="250">
	<form action="javascript:update_filter_column();">
		<table>
                        <th><td colspan="2" align="left">Search Column <span id="search_name"></span></td></th>
			<tr>
				<td>Search expression:</td>
				<td><input type="text" id="search_expression"></td>
			</tr>
			<tr>
				<td align="left" colspan="2">
					<input type="button" id="search_ok" value="OK" onClick="update_filter_column();"><input type="button" id="search_cancel" value="Cancel" onClick="return false;"></td>
			</tr>
		</table>
	</form>
</div>
        <div dojoType="LayoutContainer"
	layoutChildPriority='top-bottom'
	style="width: 100%%; height: 100%%;">
        '''

    footer="\n</div></div></body></html>"
    def make_menu_javascript(self,query):
        """ Creates the javascript required to generate the menu """
        ## Find all families:
        module_list = Registry.REPORTS.get_families()
        Theme.order_families(module_list)
        menus = []

        result = ''
        for k in module_list:
            submenu_text = ''
            ## Add the reports in the family:
            report_list = Registry.REPORTS.family[k]
            for r in report_list:
                if r.hidden: continue

                submenu_text+='''<div dojoType="MenuItem2" caption="%s" onClick="update_main('%s');"></div>\n''' % (r.name,FlagFramework.query_type((),family=k,report=r.name))

            if len(submenu_text)>0:
                menus.append('<div dojoType="PopupMenu2" id="%s" toggle="wipe">%s</div> <button dojoType="dropdownButton" menuId="%s">%s</button>' % (k,submenu_text,k,k))


        return result+'''<div dojoType="ContentPane" layoutAlign="top" style="color: black; ">
		<div class="box" layoutAlign="bottom">%s</div></div>
                <div dojoType="ToolbarContainer" layoutAlign="top" id="ToolbarContainer">
                <div dojoType="Toolbar" id="toolbar"></div>
                </div>
                ''' % ('\n'.join(menus))

    def naked_render(self,data='',ui=None,title="FLAG - Forensic Log Analysis GUI. %s" % FlagFramework.flag_version):
        """ Render the ui with minimal interventions """
        result= data
        print result
        return result

    def render(self, query=FlagFramework.query_type(()), meta='',data='',next=None,previous=None,pageno=None,ui=None,title="FLAG - Forensic Log Analysis GUI. %s" % FlagFramework.flag_version):
        ## This is a little scriptlet to ensure we are loaded within
        ## dojo environment FIXME: How do we solve the link problem? 
        ## Is it possible? The problem is that the URL is not enough
        ## to specify the state because it might include stored UIs.
        result = '<script>\ntry { djConfig; } catch(err) { document.location="/";  };\n</script>'
        print data+result
        return data+result

    def raw_render(self,data='',ui=None,title="FLAG - Forensic Log Analysis GUI. %s" % FlagFramework.flag_version):
        return data

    def menu(self,flag,query):
        result=flag.ui()
        
        self.menu_javascript = self.make_menu_javascript(query)
        title="FLAG - Forensic Log Analysis GUI. %s" % FlagFramework.flag_version

        result.result+=" ".join(
            (self.header % (title),self.menu_javascript,
             '<div dojoType="ContentPane" id="main" layoutAlign="client" style="border: 5px" executeScripts="true">'))

        ## Now create the initial front page:
        result.result+="<img src='images/logo.png'>" + self.footer

        return result
