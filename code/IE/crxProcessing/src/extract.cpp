#include <crxProcessing/extract.hpp>
#include <boost/scope_exit.hpp>
#include <minizip/unzip.h>
#include <minizip/ioapi.h>
#include <boost/scoped_array.hpp>
#include <fstream>

namespace crx {

namespace detail {

class CRXFile
{
public:
  static CRXFile* create(boost::filesystem::path aPath, std::string aMode)
  {
    try {
      return new CRXFile(aPath, aMode);
    } catch (std::exception &) {
      return NULL;
    }
  }

  static int destroy(CRXFile* aFile)
  {
    delete aFile;
    return 0;
  }

public:

  struct Header
  {
    char magic[4];
    uint32_t version;
    uint32_t pubKeyLength;
    uint32_t signatureLength;
  };

  CRXFile(boost::filesystem::wpath aPath, std::string aMode)
  {
    mFile = fopen(aPath.string().c_str(), aMode.c_str());
    if (mFile == NULL) {
      throw std::runtime_error("File not opened");
    }
    Header header;
    size_t size = fread(&header, sizeof(Header), 1, mFile);
    assert(size == 1);

    mOffset = sizeof(Header) + header.pubKeyLength + header.signatureLength;

    /*boost::scoped_array<char> pubKey(new char[header.pubKeyLength]);
    boost::scoped_array<char> signature(new char[header.signatureLength]);
    size = fread(pubKey.get(), sizeof(char), header.pubKeyLength, mFile);
    if (size != header.pubKeyLength) {
      throw "Error";
    }
    size = fread(signature.get(), sizeof(char), header.signatureLength, mFile);
    if (size != header.signatureLength) {
      throw "Error";
    }*/
    fseek(mFile, mOffset, SEEK_SET);
  }

  ~CRXFile()
  {
    if (mFile) {
      fclose(mFile);
    }
  }

  size_t read(unsigned char* aBuff, size_t aSize)
  {
    return fread(aBuff, sizeof(unsigned char), aSize, mFile);
  }

  size_t write(const unsigned char* aBuff, size_t aSize)
  {
    return fwrite(aBuff, sizeof(unsigned char), aSize, mFile);
  }

  size_t zipPosition() const
  {
    return ftell(mFile) - mOffset;
  }

  int zipSeek(ZPOS64_T offset, int origin)
  {
    if (origin == SEEK_SET) {
      return fseek(mFile, offset + mOffset, SEEK_SET);
    }

    return fseek(mFile, offset, origin);
  }

  int error()
  {
    return ferror(mFile);
  }
protected:
  FILE *mFile;
  ZPOS64_T mOffset;
};


static voidpf ZCALLBACK fopen64_file_func (voidpf opaque, const void* filename, int mode)
{
    CRXFile* file = NULL;
    const char* mode_fopen = NULL;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL)) {
      file = CRXFile::create((const wchar_t*)filename, mode_fopen);
    }
    return file;
}

static int ZCALLBACK fclose_file_func(voidpf opaque, voidpf stream)
{
    int ret;
    ret = CRXFile::destroy(reinterpret_cast<CRXFile*>(stream)); //fclose((FILE *)stream);
    return ret;
}

static uLong ZCALLBACK fread_file_func (voidpf opaque, voidpf stream, void* buf, uLong size)
{
    uLong ret;
    ret = reinterpret_cast<CRXFile*>(stream)->read(reinterpret_cast<unsigned char*>(buf), size); //in bytes
    return ret;
}

static uLong ZCALLBACK fwrite_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    uLong ret;
    ret = reinterpret_cast<CRXFile*>(stream)->write(reinterpret_cast<const unsigned char*>(buf), size); //in bytes
    return ret;
}

static ZPOS64_T ZCALLBACK ftell64_file_func (voidpf opaque, voidpf stream)
{
    ZPOS64_T ret;
    ret = reinterpret_cast<CRXFile*>(stream)->zipPosition();
    return ret;
}

static long ZCALLBACK fseek64_file_func (voidpf  opaque, voidpf stream, ZPOS64_T offset, int origin)
{
    int fseek_origin=0;
    long ret = 0;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
    }
    ret = reinterpret_cast<CRXFile*>(stream)->zipSeek(offset, fseek_origin);
    return ret;
}



static int ZCALLBACK ferror_file_func (voidpf opaque, voidpf stream)
{
    int ret;
    ret = reinterpret_cast<CRXFile*>(stream)->error();
    return ret;
}

//-----------------------------------------------------------------------

} //namespace detail
//-----------------------------------------------------------

void extractCurrentFile(unzFile aFileHandle, const boost::filesystem::wpath &aOutputPath)
{
  char filename_inzip[256];

  unz_file_info64 file_info;
  int err = UNZ_OK;
  err = unzGetCurrentFileInfo64(aFileHandle, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
  if (err != UNZ_OK){
    throw "Error";
  }
  boost::filesystem::wpath p = (aOutputPath / filename_inzip).make_preferred();
  boost::filesystem::wpath filename = p.filename();
  if (!p.has_filename() || filename == ".") {
    boost::filesystem::create_directory(p);
  } else {
    static const size_t sBufferSize = 1024;
    boost::filesystem::wpath parentPath = p.parent_path();
    if (!boost::filesystem::exists(parentPath)) {
      boost::filesystem::create_directories(p);
    }

    err = unzOpenCurrentFile(aFileHandle);
    if (err != UNZ_OK){
      throw "Error";
    }
    BOOST_SCOPE_EXIT_ALL(=) { unzCloseCurrentFile(aFileHandle); };
    boost::scoped_array<char> buffer(new char[sBufferSize]);

    std::ofstream outputStream(p.string().c_str(), std::ofstream::out | std::ofstream::binary);
    //Enable exceptions for stream errors
    outputStream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    int bytes = 0;
    do {
      bytes = unzReadCurrentFile(aFileHandle, buffer.get(), sBufferSize);

      if (bytes > 0) {
        outputStream.write(buffer.get(), bytes);
      }
    } while (bytes > 0);
    if (bytes < 0) {
      throw "Error";
    }
    outputStream.flush();
  }
}


void extract(const boost::filesystem::wpath &aCRXFilePath, const boost::filesystem::wpath &aOutputPath)
{
  unzFile crxFileHandle = NULL;

  zlib_filefunc64_def helperFunctions;

  helperFunctions.zopen64_file = detail::fopen64_file_func;
  helperFunctions.zread_file = detail::fread_file_func;
  helperFunctions.zwrite_file = detail::fwrite_file_func;
  helperFunctions.ztell64_file = detail::ftell64_file_func;
  helperFunctions.zseek64_file = detail::fseek64_file_func;
  helperFunctions.zclose_file = detail::fclose_file_func;
  helperFunctions.zerror_file = detail::ferror_file_func;
  helperFunctions.opaque = NULL;


  crxFileHandle = unzOpen2_64(aCRXFilePath.wstring().c_str(), &helperFunctions);

  if (!crxFileHandle) {
    throw "Error";
  }

  BOOST_SCOPE_EXIT_ALL(=) {
    if (UNZ_OK != unzClose(crxFileHandle)) {
        throw "Error";
    }
  };
  int err = UNZ_OK;
  unz_global_info64 globalInfo;
  err = unzGetGlobalInfo64(crxFileHandle, &globalInfo);
  if (err != UNZ_OK){
    throw "Error";
  }

  for (int i = 0; i < globalInfo.number_entry; ++i) {
      extractCurrentFile(crxFileHandle, aOutputPath);

      if ((i+1) < globalInfo.number_entry) {
          err = unzGoToNextFile(crxFileHandle);
          if (err != UNZ_OK) {
              throw "Error";
          }
      }
  }



  /*int ZEXPORT unzGoToFirstFile OF((unzFile file));
  int ZEXPORT unzGoToNextFile OF((unzFile file));*/
}

//int do_extract_currentfile(unzFile uf,
//    const int* popt_extract_without_path,
//    int* popt_overwrite,
//    const char* password)
//{
//    char filename_inzip[256];
//    char* filename_withoutpath;
//    char* p;
//    int err=UNZ_OK;
//    FILE *fout=NULL;
//    void* buf;
//    uInt size_buf;
//
//    unz_file_info64 file_info;
//    uLong ratio=0;
//    err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);
//
//    if (err!=UNZ_OK)
//    {
//        printf("error %d with zipfile in unzGetCurrentFileInfo\n",err);
//        return err;
//    }
//
//    size_buf = WRITEBUFFERSIZE;
//    buf = (void*)malloc(size_buf);
//    if (buf==NULL)
//    {
//        printf("Error allocating memory\n");
//        return UNZ_INTERNALERROR;
//    }
//
//    p = filename_withoutpath = filename_inzip;
//    while ((*p) != '\0')
//    {
//        if (((*p)=='/') || ((*p)=='\\'))
//            filename_withoutpath = p+1;
//        p++;
//    }
//
//    if ((*filename_withoutpath)=='\0')
//    {
//        if ((*popt_extract_without_path)==0)
//        {
//            printf("creating directory: %s\n",filename_inzip);
//            mymkdir(filename_inzip);
//        }
//    }
//    else
//    {
//        const char* write_filename;
//        int skip=0;
//
//        if ((*popt_extract_without_path)==0)
//            write_filename = filename_inzip;
//        else
//            write_filename = filename_withoutpath;
//
//        err = unzOpenCurrentFilePassword(uf,password);
//        if (err!=UNZ_OK)
//        {
//            printf("error %d with zipfile in unzOpenCurrentFilePassword\n",err);
//        }
//
//        if (((*popt_overwrite)==0) && (err==UNZ_OK))
//        {
//            char rep=0;
//            FILE* ftestexist;
//            ftestexist = fopen64(write_filename,"rb");
//            if (ftestexist!=NULL)
//            {
//                fclose(ftestexist);
//                do
//                {
//                    char answer[128];
//                    int ret;
//
//                    printf("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ",write_filename);
//                    ret = scanf("%1s",answer);
//                    if (ret != 1)
//                    {
//                       exit(EXIT_FAILURE);
//                    }
//                    rep = answer[0] ;
//                    if ((rep>='a') && (rep<='z'))
//                        rep -= 0x20;
//                }
//                while ((rep!='Y') && (rep!='N') && (rep!='A'));
//            }
//
//            if (rep == 'N')
//                skip = 1;
//
//            if (rep == 'A')
//                *popt_overwrite=1;
//        }
//
//        if ((skip==0) && (err==UNZ_OK))
//        {
//            fout=fopen64(write_filename,"wb");
//
//            /* some zipfile don't contain directory alone before file */
//            if ((fout==NULL) && ((*popt_extract_without_path)==0) &&
//                                (filename_withoutpath!=(char*)filename_inzip))
//            {
//                char c=*(filename_withoutpath-1);
//                *(filename_withoutpath-1)='\0';
//                makedir(write_filename);
//                *(filename_withoutpath-1)=c;
//                fout=fopen64(write_filename,"wb");
//            }
//
//            if (fout==NULL)
//            {
//                printf("error opening %s\n",write_filename);
//            }
//        }
//
//        if (fout!=NULL)
//        {
//            printf(" extracting: %s\n",write_filename);
//
//            do
//            {
//                err = unzReadCurrentFile(uf,buf,size_buf);
//                if (err<0)
//                {
//                    printf("error %d with zipfile in unzReadCurrentFile\n",err);
//                    break;
//                }
//                if (err>0)
//                    if (fwrite(buf,err,1,fout)!=1)
//                    {
//                        printf("error in writing extracted file\n");
//                        err=UNZ_ERRNO;
//                        break;
//                    }
//            }
//            while (err>0);
//            if (fout)
//                    fclose(fout);
//
//            if (err==0)
//                change_file_date(write_filename,file_info.dosDate,
//                                 file_info.tmu_date);
//        }
//
//        if (err==UNZ_OK)
//        {
//            err = unzCloseCurrentFile (uf);
//            if (err!=UNZ_OK)
//            {
//                printf("error %d with zipfile in unzCloseCurrentFile\n",err);
//            }
//        }
//        else
//            unzCloseCurrentFile(uf); /* don't lose the error */
//    }
//
//    free(buf);
//    return err;
//}

} //namespace crx
