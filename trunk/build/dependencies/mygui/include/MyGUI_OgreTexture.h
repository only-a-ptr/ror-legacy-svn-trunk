/*!
	@file
	@author		Albert Semenov
	@date		04/2009
	@module
*/

#ifndef __MYGUI_OGRE_TEXTURE_H__
#define __MYGUI_OGRE_TEXTURE_H__

#include "MyGUI_Prerequest.h"
#include "MyGUI_ITexture.h"
#include "MyGUI_RenderFormat.h"

#include <OgreResource.h>
#include <OgreTexture.h>

#include "MyGUI_LastHeader.h"

namespace MyGUI
{

	class OgreTexture :
		public ITexture,
		public Ogre::ManualResourceLoader
	{
	public:
		OgreTexture(const std::string& _name, const std::string& _group);
		virtual ~OgreTexture();

		virtual const std::string& getName();

		virtual void createManual(int _width, int _height, TextureUsage _usage, PixelFormat _format);
		virtual void loadFromFile(const std::string& _filename);
		virtual void saveToFile(const std::string& _filename);

		virtual void setInvalidateListener(ITextureInvalidateListener* _listener);

		virtual void destroy();

		virtual void* lock(TextureUsage _access);
		virtual void unlock();
		virtual bool isLocked();

		virtual int getWidth();
		virtual int getHeight();

		virtual PixelFormat getFormat() { return mOriginalFormat; }
		virtual TextureUsage getUsage() { return mOriginalUsage; }
		virtual size_t getNumElemBytes() { return mNumElemBytes; }

		virtual IRenderTarget* getRenderTarget();

	/*internal:*/
		Ogre::TexturePtr getOgreTexture() { return mTexture; }

	private:
		void setUsage(TextureUsage _usage);
		void setFormat(PixelFormat _format);
		void setFormatByOgreTexture();

		virtual void loadResource(Ogre::Resource* resource);

	private:
		Ogre::TexturePtr mTexture;
		std::string mName;
		std::string mGroup;

		TextureUsage mOriginalUsage;
		PixelFormat mOriginalFormat;
		size_t mNumElemBytes;

		Ogre::PixelFormat mPixelFormat;
		Ogre::TextureUsage mUsage;

		ITextureInvalidateListener* mListener;
		IRenderTarget* mRenderTarget;
	};

} // namespace MyGUI

#endif // __MYGUI_OGRE_TEXTURE_H__
