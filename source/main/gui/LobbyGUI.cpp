/*
This source file is part of Rigs of Rods
Copyright 2005-2011 Pierre-Michel Ricordel
Copyright 2007-2011 Thomas Fischer

For more information, see http://www.rigsofrods.com/

Rigs of Rods is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3, as 
published by the Free Software Foundation.

Rigs of Rods is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Rigs of Rods.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef USE_MYGUI 

#include "LobbyGUI.h"
#include "Scripting.h"
#include "InputEngine.h"
#include "OgreLogManager.h"

#include "Settings.h"
#include "RoRFrameListener.h"
#include "network.h"

#include <MyGUI.h>

#include "libircclient.h"

#include <boost/algorithm/string/replace.hpp> // for replace_all

// class

LobbyGUI::LobbyGUI() : current_tab(0), rotatingWait(0)
{
	initialiseByAttributes(this);

	commandBox->eventKeyButtonPressed += MyGUI::newDelegate(this, &LobbyGUI::eventButtonPressed);
	commandBox->eventEditSelectAccept += MyGUI::newDelegate(this, &LobbyGUI::eventCommandAccept);
	tabControl->eventTabChangeSelect  += MyGUI::newDelegate(this, &LobbyGUI::eventChangeTab);

	initIRC();
}

LobbyGUI::~LobbyGUI()
{
}

void LobbyGUI::setVisible( bool _visible )
{
	mainWindow->setVisible(_visible);
}

bool LobbyGUI::getVisible()
{
	return mainWindow->getVisible();
}

/*
void LobbyGUI::eventCommandAccept(MyGUI::Edit* _sender)
{
	MyGUI::UString message = _sender->getCaption();
	if(!sendMessage(message))
	{
		std::string fmt = "<" + nick + "> " + message;
		addTextToChatWindow(fmt);
	} else
	{
		addTextToChatWindow("cannot send to channel");
	}
	_sender->setCaption("");
}
*/

void LobbyGUI::addTab(Ogre::String name)
{
	/*
	<Widget type="TabItem" skin="" position="2 24 304 193">
	<Property key="Caption" value="Log"/>
	<Widget type="EditBox" skin="EditBoxStretch" position="0 0 306 195" align="Stretch" name="OgreLog">
	<Property key="WordWrap" value="true"/>
	<Property key="TextAlign" value="Left Bottom"/>
	<Property key="ReadOnly" value="true"/>
	<Property key="OverflowToTheLeft" value="true"/>
	<Property key="MaxTextLength" value="8192"/>
	<Property key="FontName" value="Default"/>
	</Widget>
	</Widget>
	*/

	Ogre::String realName = name;

	boost::algorithm::replace_all(name, "#", "##");

	tabctx_t *t = &tabs[name];

	// create tab
	t->tab = tabControl->addItem(name);
	
	t->name = name;
	t->realName = realName;

	t->tab->setProperty("Caption", name);
	t->tab->setCaption(name);
	
	// and the textbox inside
	t->txt = MyGUI::Gui::getInstance().createWidget<MyGUI::EditBox>("EditBoxStretch", 0, 0, 304, 193,  MyGUI::Align::Stretch, "txt"+name);
	t->txt->setProperty("WordWrap", "true");
	t->txt->setProperty("TextAlign", "Left Bottom");
	t->txt->setProperty("ReadOnly", "true");
	t->txt->setProperty("OverflowToTheLeft", "true");
	t->txt->setProperty("MaxTextLength", "8192");
	t->txt->setWidgetStyle(MyGUI::WidgetStyle::Child);

	// attach it to the tab
	t->txt->detachFromWidget();
	t->txt->attachToWidget(t->tab);


	t->txt->setSize(t->tab->getSize().width, t->tab->getSize().height);

	//mTabControl->setItemData(t->tab, t);
	t->tab->setUserData(t);

	t->mHistoryPosition = 0;
	t->mHistory.push_back("");

	// set focus to new tab
	tabControl->selectSheetIndex(tabs.size()-1, false);
	current_tab = t;
}

void LobbyGUI::processIRCEvent(message_t &msg)
{
	// cutoff the nickname after the !
	if(!msg.nick.empty())
	{
		size_t s = msg.nick.find("!");
		if(s != msg.nick.npos)
		{
			msg.nick = msg.nick.substr(0, s);
		}
	}
	
	switch(msg.type)
	{
	case MT_Channel:
		{
			std::string fmt = "<" + msg.nick + "> " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_JoinChannelSelf:
		{
			std::string fmt = "joined channel " + msg.channel + " as " + msg.nick + "";
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_JoinChannelOther:
		{
			std::string fmt = "*** " + msg.nick + " has joined " + msg.channel;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_PartChannelOther:
		{
			std::string fmt = "*** " + msg.nick + " has left " + msg.channel;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_QuitOther:
		{
			std::string fmt = "*** " + msg.nick + " has quit (" + msg.message + ")";
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_PartChannelSelf:
		{
			std::string fmt = "*** you left the channel " + msg.channel;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_ChangedChannelTopic:
		{
			std::string fmt = "*** " + msg.nick + " has changed the channel topic of " + msg.channel + " to " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_WeGotKicked:
		{
			std::string fmt = "*** we got kicked from " + msg.channel + " with the reason " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_SomeoneGotKicked:
		{
			std::string fmt = "*** " + msg.nick + " got kicked from " + msg.channel + " with the reason " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_GotPrivateMessage:
		{
			std::string fmt = "(private) <" + msg.nick + "> " + msg.message;
			addTextToChatWindow(fmt, msg.nick);
		}
		break;
	case MT_GotNotice:
	case MT_VerboseMessage:
		{
			std::string fmt;
			if(msg.nick.empty())
				fmt = "(NOTICE) " + msg.message;
			else
				fmt = "(NOTICE) <" + msg.nick + "> " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_GotInvitation:
		{
			std::string fmt = "*** " + msg.nick + " invited you to channel " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_TopicInfo:
		{
			std::string fmt = "*** Topic is " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_ErrorAuth:
		{
			std::string fmt = "*** ERROR: " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
		
	case MT_NameList:
		{
			/*
			Ogre::StringVector v = Ogre::StringUtil::split(msg.message, " ");
			for(int i=0; i<v.size(); i++)
			{

			}
			*/
			std::string fmt = "*** Users are: " + msg.message;
			addTextToChatWindow(fmt, msg.channel);
		}
		break;
	case MT_StatusUpdate:
		{
			statusText->setCaption(msg.message);
			if(msg.arg.empty())
			{
				waitingAnimation = false;
				waitDisplay->setVisible(false);
			} else
			{
				waitingAnimation = true;
				waitDisplay->setVisible(true);
			}

		}
		break;

		
	}
}

void LobbyGUI::addTextToChatWindow(std::string txt, std::string channel)
{
	//catch special case that channel is empty -> Status Channel
	if(channel.empty())
		channel = "Status";
	Ogre::String realchannel = channel;

	// escape #
	boost::algorithm::replace_all(channel, "#", "##");
	boost::algorithm::replace_all(txt, "#", "##");


	if(tabs.find(channel) == tabs.end())
	{
		// add a new tab
		addTab(realchannel);
	}

	MyGUI::Edit* ec = tabs[channel].txt;
	if (!ec->getCaption().empty())
		ec->addText("\n" + txt);
	else
		ec->addText(txt);

	ec->setTextSelection(ec->getTextLength(), ec->getTextLength());	
}

void LobbyGUI::update(float dt)
{
	if(!rotatingWait)
	{
		MyGUI::ISubWidget* waitDisplaySub = waitDisplay->getSubWidgetMain();
		rotatingWait = waitDisplaySub->castType<MyGUI::RotatingSkin>();

		/* 
		we need to patch mygui for this D:
		MyGUI::IntSize s = waitDisplay->getImageSize();
		rotatingWait->setCenter(MyGUI::IntPoint(s.width*0.5f,s.height*0.5f));
		*/
		// so instead hardcode stuff ...
		rotatingWait->setCenter(MyGUI::IntPoint(8,8));
	}

	if(waitingAnimation)
	{
		// rotate the hour glass
		float angle = rotatingWait->getAngle();
		angle += dt * 0.001f;
		if(angle > 2 * Ogre::Math::PI) angle -= 2 * Ogre::Math::PI; // prevent overflowing
		rotatingWait->setAngle(angle);
	}

	process();
}

void LobbyGUI::eventButtonPressed(MyGUI::Widget* _sender, MyGUI::KeyCode _key, MyGUI::Char _char)
{
	if(!current_tab) return;
	if(_key == MyGUI::KeyCode::Escape || _key == MyGUI::KeyCode::Enum(INPUTENGINE.getKeboardKeyForCommand(EV_COMMON_CONSOLEDISPLAY)))
	{
		setVisible(false);
		// delete last character (to avoid printing `)
		size_t lastChar = commandBox->getTextLength() - 1;
		if (_key != MyGUI::KeyCode::Escape && commandBox->getCaption()[lastChar] == '`')
			commandBox->eraseText(lastChar);
		return;
	}

	switch(_key.toValue())
	{
	case MyGUI::KeyCode::ArrowUp:
		if(current_tab->mHistoryPosition > 0)
		{
			// first we save what we was writing
			if (current_tab->mHistoryPosition == (int)current_tab->mHistory.size() - 1)
			{
				current_tab->mHistory[current_tab->mHistoryPosition] = commandBox->getCaption();
			}
			current_tab->mHistoryPosition--;
			commandBox->setCaption(current_tab->mHistory[current_tab->mHistoryPosition]);
		}
		break;

	case MyGUI::KeyCode::ArrowDown:
		if(current_tab->mHistoryPosition < (int)current_tab->mHistory.size() - 1)
		{
			current_tab->mHistoryPosition++;
			commandBox->setCaption(current_tab->mHistory[current_tab->mHistoryPosition]);
		}
		break;

	case MyGUI::KeyCode::PageUp:
		if (current_tab->txt->getVScrollPosition() > (size_t)current_tab->txt->getHeight())
			current_tab->txt->setVScrollPosition(current_tab->txt->getVScrollPosition() - current_tab->txt->getHeight());
		else
			current_tab->txt->setVScrollPosition(0);
		break;

	case MyGUI::KeyCode::PageDown:
		current_tab->txt->setVScrollPosition(current_tab->txt->getVScrollPosition() + current_tab->txt->getHeight());
		break;
	}
}

void LobbyGUI::eventCommandAccept(MyGUI::Edit* _sender)
{
	if(!current_tab) return;
	Ogre::String command = _sender->getCaption();

	Ogre::StringUtil::trim(command);

	if(command.empty())
	{
		commandBox->setCaption("");
		return;
	}

	// unescape #
	boost::algorithm::replace_all(command, "##", "#");

	if(current_tab->name.substr(0, 1) == "#")
	{
		// its a channel, send message there

		// check if its a command
		if(command.substr(0, 1) == "/")
		{
			// command there!
			Ogre::StringVector v = Ogre::StringUtil::split(command, " ");

			if(v.size() >= 2 && v[0] == "/me")
				sendMeMessage(v[1], v.size()>2?v[2]:current_tab->name);

			else if(v.size() >= 2 && v[0] == "/PRIVMSG")
				sendMessage(v[1], v.size()>2?v[2]:current_tab->name);

			else if(v.size() >= 2 && (v[0] == "/join" || v[0] == "/j"))
				joinChannel(v[1], v.size()>2?v[2]:"");

			else if(v.size() >= 2 && (v[0] == "/nick" || v[0] == "/n"))
				changeNick(v[1]);

			else if(v.size() >= 2 && (v[0] == "/leave" || v[0] == "/l"))
				leaveChannel(v[1]);

			else if(v.size() >= 2 && (v[0] == "/quit" || v[0] == "/q"))
				quit(v.size()>1?v[1]:"");

		} else
		{
			// no command
			sendMessage(command, current_tab->realName);
		}
		// add our message to the textbox
		std::string fmt = "<" + nick + "> " + command;
		addTextToChatWindow(fmt, current_tab->realName); // use realName here!
	}

	// save some history
	*current_tab->mHistory.rbegin() = command;
	current_tab->mHistory.push_back(""); // new, empty last entry
	current_tab->mHistoryPosition = current_tab->mHistory.size() - 1; // switch to the new line
	commandBox->setCaption(current_tab->mHistory[current_tab->mHistoryPosition]);
}

void LobbyGUI::eventChangeTab(MyGUI::TabControl* _sender, size_t _index)
{
	MyGUI::TabItemPtr tab = _sender->getItemAt(_index);
	String n = _sender->getItemNameAt(_index);
	if(!tab) return;
	if(tabs.find(n) == tabs.end())
		return;

	bool enabled = true;
	if(n == "Status")  enabled = false;
	commandBox->setEnabled(enabled);

	current_tab = &tabs[n];
}



#endif //USE_MYGUI 