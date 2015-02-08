#pragma once

#ifdef WIN32
#include <windows.h>
#endif
#include <stddef.h>
#include <string>
#include <vector>
#include <exception>
#include "binaryninjacore.h"

#ifdef _MSC_VER
#define NOEXCEPT
#else
#define NOEXCEPT noexcept
#endif


namespace BinaryNinja
{
	class RefCountObject
	{
	public:
		int m_refs;
		RefCountObject(): m_refs(0) {}
		virtual ~RefCountObject() {}

		void AddRef()
		{
	#ifdef WIN32
			InterlockedIncrement((LONG*)&m_refs);
	#else
			__sync_fetch_and_add(&m_refs, 1);
	#endif
		}

		void Release()
		{
	#ifdef WIN32
			if (InterlockedDecrement((LONG*)&m_refs) == 0)
				delete this;
	#else
			if (__sync_fetch_and_add(&m_refs, -1) == 1)
				delete this;
	#endif
		}
	};

	template <class T>
	class Ref
	{
		T* m_obj;

	public:
		Ref<T>(): m_obj(NULL)
		{
		}

		Ref<T>(T* obj): m_obj(obj)
		{
			if (m_obj)
				m_obj->AddRef();
		}

		Ref<T>(const Ref<T>& obj): m_obj(obj.m_obj)
		{
			if (m_obj)
				m_obj->AddRef();
		}

		~Ref<T>()
		{
			if (m_obj)
				m_obj->Release();
		}

		Ref<T>& operator=(const Ref<T>& obj)
		{
			T* oldObj = m_obj;
			m_obj = obj.m_obj;
			if (m_obj)
				m_obj->AddRef();
			if (oldObj)
				oldObj->Release();
			return *this;
		}

		Ref<T>& operator=(T* obj)
		{
			T* oldObj = m_obj;
			m_obj = obj;
			if (m_obj)
				m_obj->AddRef();
			if (oldObj)
				oldObj->Release();
			return *this;
		}

		operator T*() const
		{
			return m_obj;
		}

		T* operator->() const
		{
			return m_obj;
		}

		T& operator*() const
		{
			return *m_obj;
		}

		bool operator!() const
		{
			return m_obj == NULL;
		}

		T* GetPtr() const
		{
			return m_obj;
		}
	};

	class DataBuffer
	{
		BNDataBuffer* m_buffer;

	public:
		DataBuffer();
		DataBuffer(size_t len);
		DataBuffer(const DataBuffer& buf);
		DataBuffer(BNDataBuffer* buf);
		~DataBuffer();

		DataBuffer& operator=(const DataBuffer& buf);

		BNDataBuffer* GetBufferObject() const { return m_buffer; }

		void* GetData();
		const void* GetData() const;
		void* GetDataAt(size_t offset);
		size_t GetLength() const;

		void SetSize(size_t len);
		void Append(const void* data, size_t len);
		void Append(const DataBuffer& buf);

		DataBuffer GetSlice(size_t start, size_t len);

		uint8_t& operator[](size_t offset);
		const uint8_t& operator[](size_t offset) const;
	};

	class NavigationHandler
	{
	private:
		BNNavigationHandler m_callbacks;

		static char* GetCurrentViewCallback(void* ctxt);
		static uint64_t GetCurrentOffsetCallback(void* ctxt);
		static bool NavigateCallback(void* ctxt, const char* view, uint64_t offset);

	public:
		NavigationHandler();
		virtual ~NavigationHandler() {}

		BNNavigationHandler* GetCallbacks() { return &m_callbacks; }

		virtual std::string GetCurrentView() = 0;
		virtual uint64_t GetCurrentOffset() = 0;
		virtual bool Navigate(const std::string& view, uint64_t offset) = 0;
	};

	class UndoAction
	{
	private:
		static void UndoCallback(void* ctxt);
		static void RedoCallback(void* ctxt);

	public:
		virtual ~UndoAction() {}

		void Add(BNFileMetadata* file);

		virtual void Undo() = 0;
		virtual void Redo() = 0;
	};

	class FileMetadata: public RefCountObject
	{
		BNFileMetadata* m_file;

	public:
		FileMetadata();
		FileMetadata(BNFileMetadata* file);
		~FileMetadata();

		BNFileMetadata* GetFileObject() const { return m_file; }

		void SetNavigationHandler(NavigationHandler* handler);

		bool IsModified() const;
		void MarkFileModified();
		void MarkFileSaved();

		void BeginUndoActions();
		void AddUndoAction(UndoAction* action);
		void CommitUndoActions();

		bool Undo();
		bool Redo();

		std::string GetCurrentView();
		uint64_t GetCurrentOffset();
		bool Navigate(const std::string& view, uint64_t offset);
	};

	class BinaryView;

	class BinaryDataNotification
	{
	private:
		BNBinaryDataNotification m_callbacks;

		static void DataWrittenCallback(void* ctxt, BNBinaryView* data, uint64_t offset, size_t len);
		static void DataInsertedCallback(void* ctxt, BNBinaryView* data, uint64_t offset, size_t len);
		static void DataRemovedCallback(void* ctxt, BNBinaryView* data, uint64_t offset, uint64_t len);

	public:
		BinaryDataNotification();
		virtual ~BinaryDataNotification() {}

		BNBinaryDataNotification* GetCallbacks() { return &m_callbacks; }

		virtual void OnBinaryDataWritten(BinaryView* view, uint64_t offset, size_t len) = 0;
		virtual void OnBinaryDataInserted(BinaryView* view, uint64_t offset, size_t len) = 0;
		virtual void OnBinaryDataRemoved(BinaryView* view, uint64_t offset, uint64_t len) = 0;
	};

	class FileAccessor
	{
	protected:
		BNFileAccessor m_callbacks;

	private:
		static uint64_t GetLengthCallback(void* ctxt);
		static size_t ReadCallback(void* ctxt, void* dest, uint64_t offset, size_t len);
		static size_t WriteCallback(void* ctxt, uint64_t offset, const void* src, size_t len);

	public:
		FileAccessor();
		FileAccessor(BNFileAccessor* accessor);
		virtual ~FileAccessor() {}

		BNFileAccessor* GetCallbacks() { return &m_callbacks; }

		virtual bool IsValid() const = 0;
		virtual uint64_t GetLength() const = 0;
		virtual size_t Read(void* dest, uint64_t offset, size_t len) = 0;
		virtual size_t Write(uint64_t offset, const void* src, size_t len) = 0;
	};

	class CoreFileAccessor: public FileAccessor
	{
	public:
		CoreFileAccessor(BNFileAccessor* accessor);

		virtual bool IsValid() const override { return true; }
		virtual uint64_t GetLength() const override;
		virtual size_t Read(void* dest, uint64_t offset, size_t len) override;
		virtual size_t Write(uint64_t offset, const void* src, size_t len) override;
	};

	class BinaryView: public RefCountObject
	{
	protected:
		BNBinaryView* m_view;
		Ref<FileMetadata> m_file;

		BinaryView(FileMetadata* file);
		BinaryView(BNBinaryView* view);

	private:
		static size_t ReadCallback(void* ctxt, void* dest, uint64_t offset, size_t len);
		static size_t WriteCallback(void* ctxt, uint64_t offset, const void* src, size_t len);
		static size_t InsertCallback(void* ctxt, uint64_t offset, const void* src, size_t len);
		static size_t RemoveCallback(void* ctxt, uint64_t offset, uint64_t len);
		static BNModificationStatus GetModificationCallback(void* ctxt, uint64_t offset);
		static uint64_t GetStartCallback(void* ctxt);
		static uint64_t GetLengthCallback(void* ctxt);
		static bool SaveCallback(void* ctxt, BNFileAccessor* file);

	public:
		virtual ~BinaryView();

		FileMetadata* GetFile() const { return m_file; }
		BNBinaryView* GetViewObject() const { return m_view; }

		bool IsModified() const;

		void BeginUndoActions();
		void AddUndoAction(UndoAction* action);
		void CommitUndoActions();

		bool Undo();
		bool Redo();

		std::string GetCurrentView();
		uint64_t GetCurrentOffset();
		bool Navigate(const std::string& view, uint64_t offset);

		virtual size_t Read(void* dest, uint64_t offset, size_t len) = 0;
		DataBuffer ReadBuffer(uint64_t offset, size_t len);

		virtual size_t Write(uint64_t offset, const void* data, size_t len) { (void)offset; (void)data; (void)len; return 0; }
		size_t WriteBuffer(uint64_t offset, const DataBuffer& data);

		virtual size_t Insert(uint64_t offset, const void* data, size_t len) { (void)offset; (void)data; (void)len; return 0; }
		size_t InsertBuffer(uint64_t offset, const DataBuffer& data);

		virtual size_t Remove(uint64_t offset, uint64_t len) { (void)offset; (void)len; return 0; }

		virtual BNModificationStatus GetModification(uint64_t offset) { (void)offset; return Original; }
		std::vector<BNModificationStatus> GetModification(uint64_t offset, size_t len);

		virtual uint64_t GetStart() const { return 0; }
		uint64_t GetEnd() const;
		virtual uint64_t GetLength() const = 0;

		virtual bool Save(FileAccessor* file) { (void)file; return false; }
		bool Save(const std::string& path);

		void RegisterNotification(BinaryDataNotification* notify);
		void UnregisterNotification(BinaryDataNotification* notify);
	};

	class CoreBinaryView: public BinaryView
	{
	public:
		CoreBinaryView(BNBinaryView* view);

		virtual size_t Read(void* dest, uint64_t offset, size_t len) override;
		virtual size_t Write(uint64_t offset, const void* data, size_t len) override;
		virtual size_t Insert(uint64_t offset, const void* data, size_t len) override;
		virtual size_t Remove(uint64_t offset, uint64_t len) override;

		virtual BNModificationStatus GetModification(uint64_t offset) override;

		virtual uint64_t GetStart() const override;
		virtual uint64_t GetLength() const override;

		virtual bool Save(FileAccessor* file) override;
	};

	class BinaryData: public CoreBinaryView
	{
	public:
		BinaryData(FileMetadata* file);
		BinaryData(FileMetadata* file, const DataBuffer& data);
		BinaryData(FileMetadata* file, const void* data, size_t len);
		BinaryData(FileMetadata* file, const std::string& path);
		BinaryData(FileMetadata* file, FileAccessor* accessor);
	};

	class ReadException: public std::exception
	{
	public:
		ReadException(): std::exception() {}
		virtual const char* what() const NOEXCEPT { return "read out of bounds"; }
	};

	class BinaryReader
	{
		Ref<BinaryView> m_view;
		BNBinaryReader* m_stream;

	public:
		BinaryReader(BinaryView* data, BNEndianness endian = LittleEndian);
		~BinaryReader();

		BNEndianness GetEndianness() const;
		void SetEndianness(BNEndianness endian);

		void Read(void* dest, size_t len);
		uint8_t Read8();
		uint16_t Read16();
		uint32_t Read32();
		uint64_t Read64();
		uint16_t ReadLE16();
		uint32_t ReadLE32();
		uint64_t ReadLE64();
		uint16_t ReadBE16();
		uint32_t ReadBE32();
		uint64_t ReadBE64();

		bool TryRead(void* dest, size_t len);
		bool TryRead8(uint8_t& result);
		bool TryRead16(uint16_t& result);
		bool TryRead32(uint32_t& result);
		bool TryRead64(uint64_t& result);
		bool TryReadLE16(uint16_t& result);
		bool TryReadLE32(uint32_t& result);
		bool TryReadLE64(uint64_t& result);
		bool TryReadBE16(uint16_t& result);
		bool TryReadBE32(uint32_t& result);
		bool TryReadBE64(uint64_t& result);

		uint64_t GetOffset() const;
		void Seek(uint64_t offset);
		void SeekRelative(int64_t offset);

		bool IsEndOfFile() const;
	};

	class WriteException: public std::exception
	{
	public:
		WriteException(): std::exception() {}
		virtual const char* what() const NOEXCEPT { return "write out of bounds"; }
	};

	class BinaryWriter
	{
		Ref<BinaryView> m_view;
		BNBinaryWriter* m_stream;

	public:
		BinaryWriter(BinaryView* data, BNEndianness endian = LittleEndian);
		~BinaryWriter();

		BNEndianness GetEndianness() const;
		void SetEndianness(BNEndianness endian);

		void Write(const void* src, size_t len);
		void Write8(uint8_t val);
		void Write16(uint16_t val);
		void Write32(uint32_t val);
		void Write64(uint64_t val);
		void WriteLE16(uint16_t val);
		void WriteLE32(uint32_t val);
		void WriteLE64(uint64_t val);
		void WriteBE16(uint16_t val);
		void WriteBE32(uint32_t val);
		void WriteBE64(uint64_t val);

		bool TryWrite(const void* src, size_t len);
		bool TryWrite8(uint8_t val);
		bool TryWrite16(uint16_t val);
		bool TryWrite32(uint32_t val);
		bool TryWrite64(uint64_t val);
		bool TryWriteLE16(uint16_t val);
		bool TryWriteLE32(uint32_t val);
		bool TryWriteLE64(uint64_t val);
		bool TryWriteBE16(uint16_t val);
		bool TryWriteBE32(uint32_t val);
		bool TryWriteBE64(uint64_t val);

		uint64_t GetOffset() const;
		void Seek(uint64_t offset);
		void SeekRelative(int64_t offset);
	};
}