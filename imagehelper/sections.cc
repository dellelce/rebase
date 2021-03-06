/*
 * Copyright (c) 2002 Ralf Habacker  <Ralf.Habacker@freenet.de>
 * Copyright (c) 2008 Jason Tishler  <jason@tishler.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id: sections.cc,v 1.10 2003/02/14 08:23:13 habacker Exp
 * $Id: sections.cc,v 1.3 2008/06/09 18:54:30 jt Exp $    
 */

#include <iostream>
#include <iomanip>

#include "sections.h"

int Base::debug = 0;

Section::Section(void *aFileBase, SectionHeader *p)
{
  header = p;
  adjust = (uintptr_t) header->PointerToRawData
	   + (uintptr_t) aFileBase - header->VirtualAddress;
  strncpy(Name,(char *)header->Name,8);
  Name[8] = '\0';
}

// FIXME: should be print() with returning ostream, but this isn't supported with gcc 2.

void Section::debugprint(const char *title)
{
  std::cerr << std::setw(10) << std::setfill(' ') << title \
  << " name: " << std::setw(8) << std::setfill(' ') << Name \
  << " base: 0x" << std::setw(8) << std::setfill('0') << std::hex << header->VirtualAddress << std::dec \
  << " size: 0x" << std::setw(8) << std::setfill('0') << std::hex << header->SizeOfRawData << std::dec \
  << " file offset: 0x" << std::setw(8) << std::setfill('0') << std::hex << header->PointerToRawData << std::dec \
  << " offset: 0x" << std::setw(8) << std::setfill('0') << std::hex << adjust << std::dec << std::endl;
}

void Section::print(const char *title)
{
  std::cout << std::setw(10) << std::setfill(' ') << title \
  << " name: " << std::setw(8) << std::setfill(' ') << Name \
  << " base: 0x" << std::setw(8) << std::setfill('0') << std::hex << header->VirtualAddress << std::dec \
  << " size: 0x" << std::setw(8) << std::setfill('0') << std::hex << header->SizeOfRawData << std::dec \
  << " file offset: 0x" << std::setw(8) << std::setfill('0') << std::hex << header->PointerToRawData << std::dec \
  << " offset: 0x" << std::setw(8) << std::setfill('0') << std::hex << adjust << std::dec << std::endl;
}

bool Section::isIn(Section &in)
{
  return header->VirtualAddress >= in.header->VirtualAddress && header->VirtualAddress < in.header->VirtualAddress + in.header->SizeOfRawData;
}

bool Section::isIn(uint addr)
{
  return addr >= header->VirtualAddress && addr < header->VirtualAddress + header->SizeOfRawData;
}


SectionList::SectionList(void *aFileBase)
{
  PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER) aFileBase;
  PIMAGE_NT_HEADERS32 ntheader32 = (PIMAGE_NT_HEADERS32) ((char *)dosheader + dosheader->e_lfanew);
  PIMAGE_NT_HEADERS64 ntheader64 = (PIMAGE_NT_HEADERS64) ntheader32;

  FileBase = (uintptr_t) aFileBase;
  if (ntheader32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
      header = (SectionHeader *) (ntheader64+1);
      count = ntheader64->FileHeader.NumberOfSections;
    }
  else
    {
      header = (SectionHeader *) (ntheader32+1);
      count = ntheader32->FileHeader.NumberOfSections;
    }
  for (int i = 0; i < count; i++)
    {
      sections[i] = new Section(aFileBase,&header[i]);
      if (debug)
        sections[i]->print("section");
    }
}

SectionList::~SectionList()
{
  for (int i = 0; i < count; i++)
    delete sections[i];
}

Section *SectionList::find(const char *name)
{
  for (int i = 0; i < count; i++)
    {
      if ( strstr(sections[i]->getName(),name) )
        return sections[i];
    }
  return 0;
}

Section *SectionList::find(uint address)
{
  for (int i = 0; i < count; i++)
    {
      if (sections[i]->isIn(address))
        return sections[i];
    }
  return 0;
}


//----- class Exports --------------------------------------------------

Exports::Exports(Section &asection)
{
  adjust = asection.getAdjust();
  exports = (ExportDirectory *)(asection.getStartAddress());
}

Exports::Exports(SectionList &sections, DataDirectory *iddp)
{
  Section *sec = sections.find(iddp->VirtualAddress);
  if (sec)
    {
      adjust = sec->getAdjust();
      exports = (ExportDirectory *) (iddp->VirtualAddress + adjust);
    }
  else
    {
      exports = 0;
      adjust = 0;
      //  std::cerr << __FUNCTION__ << " - error: can't find section for creating export object" << std::endl;
    }
}

uint Exports::getVirtualAddress(char *symbol, uint *ordinal)
{
  if (!exports)
    return 0;

  int n = exports->NumberOfFunctions;
  uint *p = (unsigned int *)((char *)adjust + exports->AddressOfFunctions);
  char **s = (char **)((char *)adjust + exports->AddressOfNames);
  ushort *o = (ushort *)((char *)adjust + exports->AddressOfNameOrdinals);
  for (int i = 0; i < n; i++,p++,s++)
    {
      if (strcmp(symbol,*s+adjust) == 0)
        {
          if (ordinal)
            *ordinal = *o;
          return *p;
        }
    }
  return 0;
}

void Exports::reset(void)
{
  iterator = 0;
}

char *Exports::getNext(void)
{
  if (!exports)
    return 0;

  if (iterator < exports->NumberOfNames)
    {
      char **s = (char **)((char *)adjust + exports->AddressOfNames);
      return (char *)(*(s+iterator++) + adjust);
    }
  else
    return 0;
}


void Exports::dump(const char *title)
{

  char *p;
  std::cout << "exports" << std::endl;
  if (!exports)
    std::cout << "\tno exports available" << std::endl;
  else
    {
      reset();
      while ((p = getNext()) != NULL)
        {
          std::cout << "\t" << p << std::endl;
        }
    }
}

//----- class Imports --------------------------------------------------


Imports::Imports(Section &asection)
{
  imports = (ImportDescriptor *)(asection.getStartAddress());
  adjust = asection.getAdjust();
}

Imports::Imports(SectionList &sections, DataDirectory *iddp)
{
  Section *sec = sections.find(iddp->VirtualAddress);
  if (sec)
    {
      adjust = sec->getAdjust();
      imports = (ImportDescriptor *)(iddp->VirtualAddress + adjust);
    }
  else
    {
      imports = 0;
      adjust = 0;
      //  std::cerr << __FUNCTION__ << " - error: can't find section for creating import object" << std::endl;
    }
}

void Imports::reset(void)
{
  iterator = imports;
}

ImportDescriptor *Imports::getNextDescriptor(void)
{
  if (imports)
    return iterator->Name ? iterator++ : 0;
  else
    return 0;
}

void Imports::dump(const char *title)
{
  ImportDescriptor *p;

  std::cout << "imports" << std::endl;

  reset();
  while ((p = getNextDescriptor()) != NULL)
    {
      std::cout << p->Name + adjust << std::endl;
      std::cout << "vma:           Hint     Time      Forward  DLL       First" << std::endl;
      std::cout << "               Table    Stamp     Chain    Name      Thunk" << std::endl;
      std::cout << std::setw(8) << std::setfill('0') << std::hex << 0 << std::dec << "       ";
      std::cout << std::setw(8) << std::setfill('0') << std::hex << p->OriginalFirstThunk << std::dec << " ";
      std::cout << std::setw(8) << std::setfill('0') << std::hex << p->TimeDateStamp << std::dec << "  ";
      std::cout << std::setw(8) << std::setfill('0') << std::hex << p->ForwarderChain << std::dec << " ";
      std::cout << std::setw(8) << std::setfill('0') << std::hex << p->Name << std::dec << "  ";
      std::cout << std::setw(8) << std::setfill('0') << std::hex << p->FirstThunk << std::dec << std::endl;

    }
  std::cout << std::endl;
}


//----- class Imports --------------------------------------------------


Relocations::Relocations(SectionList &sectionList, const char *sectionName)
{
  sections = &sectionList;
  Section *asection = sections->find(sectionName);
  if (asection)
    {
      relocs = PIMAGE_BASE_RELOCATION (asection->getStartAddress());
      adjust = asection->getAdjust();
      size = asection->getSize();
    }
  else
    {
      relocs = 0;
      size = 0;
    }
}

bool Relocations::check(void)
{
  PIMAGE_BASE_RELOCATION relocp = relocs;
  Section *cursec;
  int errors = 0;

  if (!relocs)
    return false;

  if (debug)
    std::cerr << "debug: checking relocations .... " << std::endl;

  for (; &relocp->SizeOfBlock < (PDWORD) ((char *)relocs + size) && relocp->SizeOfBlock != 0; relocp = (PIMAGE_BASE_RELOCATION) ((char *)relocp + relocp->SizeOfBlock))
    {
      int NumOfRelocs = (relocp->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof (WORD);
      int va = relocp->VirtualAddress;
      PWORD p = (PWORD)((uintptr_t)relocp + sizeof(IMAGE_BASE_RELOCATION));
      if (debug)
	  std::cerr << "debug: blocksize= " << std::dec << NumOfRelocs << std::endl;

      cursec = sections->find(va);
      if (debug)
        {
	  std::cerr << "debug: section= "; 
	  cursec->debugprint();
	}
      if (!cursec)
        {
          if (debug)
            std::cerr << "warning: dll is corrupted - relocations for '0x" \
            << std::setw(8) << std::setfill('0') << std::hex << va << std::dec \
            << "' are pointing to a non existent section" << std::endl;
          errors++;
          continue;
        }
      if (debug)
	for (int i = 0; i < NumOfRelocs; i++,p++)
	  {
	    int location = (*p & 0x0fff) + va;
	    std::cerr << "debug: location= 0x" << std::setw(8) << std::setfill('0') << std::hex << location << std::endl;
	  }
    }
  return errors == 0;
}

bool Relocations::fix(void)
{
  PIMAGE_BASE_RELOCATION relocp = relocs;
  Section *cursec;
  int errors = 0;

  if (!relocs)
    return false;

  if (debug)
    std::cerr << "warning: fixing bad relocations .... ";

  for (; &relocp->SizeOfBlock < (PDWORD) ((char *)relocs + size) && relocp->SizeOfBlock != 0; relocp = (PIMAGE_BASE_RELOCATION) ((char *)relocp + relocp->SizeOfBlock))
    {
      int va = relocp->VirtualAddress;

      cursec = sections->find(va);
      if (!cursec)
        {
          errors++;
          relocp->SizeOfBlock = 0;
          relocp->VirtualAddress = 0;
          break;
        }
    }
  if (errors == 0)
    {
      if (debug)
        std::cerr << "no errors found" << std::endl;
    }
  else
    if (debug)
      std::cerr << "corrupted relocation records fixed" << std::endl;
  return true;
}


bool Relocations::relocate(int64_t difference)
{
  PIMAGE_BASE_RELOCATION relocp = relocs;
  int WholeNumOfRelocs = 0;

  if (!relocs)
    return false;

  for (; &relocp->SizeOfBlock < (PDWORD) ((char *)relocs + size) && relocp->SizeOfBlock != 0; relocp = (PIMAGE_BASE_RELOCATION) ((char *)relocp + relocp->SizeOfBlock))
    {
      int NumOfRelocs = (relocp->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof (WORD);
      int va = relocp->VirtualAddress;
      PWORD p = (PWORD)((uintptr_t)relocp + sizeof(IMAGE_BASE_RELOCATION));
      if (debug)
        {
          std::cerr << "VirtAdress: 0x" \
          << std::setw(8) << std::setfill('0') << std::hex << relocp->VirtualAddress << std::dec << std::endl;
          std::cerr << "NumOfRelocs: " << NumOfRelocs << std::endl;
        }
      WholeNumOfRelocs += NumOfRelocs;

      Section *cursec = sections->find(va);
      if (!cursec)
        {
          if (debug)
            std::cerr << "warning: dll is corrupted - the relocations '0x" \
            << std::setw(8) << std::setfill('0') << std::hex << va << std::dec \
            << "' points to a non existing section and could not be relocated" << std::endl;
        	return false;
        }
      else if (debug)
        cursec->debugprint("currently relocated section");

      int adjust = cursec->getAdjust();

      for (int i = 0; i < NumOfRelocs; i++,p++)
        {
	  WORD rel_type = (*p & 0xf000) >> 12;
	  int location = (*p & 0x0fff) + va;

	  switch (rel_type)
	    {
	    case IMAGE_REL_BASED_ABSOLUTE:
	      break;
	    case IMAGE_REL_BASED_HIGHLOW:
	      {
		if (debug)
		  {
		    std::cerr << "HIGHLOW: 0x" \
		    << std::setw(8) << std::setfill('0') << std::hex << location << std::dec \
		    << " - ";
		    std::cerr << "0x" \
		    << std::setw(8) << std::setfill('0') << std::hex << location + adjust + 3 << std::dec \
		    << std::endl;
		  }
		int32_t *patch_adr = (int *)cursec->rva2real(location);
		*patch_adr += difference;
	      }
	      break;
	    case IMAGE_REL_BASED_DIR64:
	      {
		if (debug)
		  {
		    std::cerr << "  DIR64: 0x" \
		    << std::setw(16) << std::setfill('0') << std::hex << location << std::dec \
		    << " - ";
		    std::cerr << "0x" \
		    << std::setw(16) << std::setfill('0') << std::hex << location + adjust + 7 << std::dec \
		    << std::endl;
		  }
		int64_t *patch_adr = (int64_t *)cursec->rva2real(location);
		*patch_adr += difference;
	      }
	      break;
	    default:
	      std::cerr << "Unsupported relocation type " << rel_type << std::endl;
	      break;
	    }
        }
    }
  return true;
}
