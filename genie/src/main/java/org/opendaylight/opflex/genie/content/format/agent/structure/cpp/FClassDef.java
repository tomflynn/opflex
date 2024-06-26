package org.opendaylight.opflex.genie.content.format.agent.structure.cpp;

import org.opendaylight.opflex.genie.content.format.agent.meta.cpp.FMetaDef;
import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameComponent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameRule;
import org.opendaylight.opflex.genie.content.model.mnaming.MNamer;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mrelator.MRelationshipClass;
import org.opendaylight.opflex.genie.content.model.mtype.Language;
import org.opendaylight.opflex.genie.content.model.mtype.MLanguageBinding;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.content.model.mtype.PassBy;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Ident;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

import java.util.*;

/**
 * Created by midvorki on 9/28/14.
 */
public class FClassDef extends ItemFormatterTask
{
    public FClassDef(
        FormatterCtx aInFormatterCtx,
        FileNameRule aInFileNameRule,
        Indenter aInIndenter,
        BlockFormatDirective aInHeaderFormatDirective,
        BlockFormatDirective aInCommentFormatDirective,
        boolean aInIsUserFile,
        Item aInItem)
    {
        super(
             aInFormatterCtx,
             aInFileNameRule,
             aInIndenter,
             aInHeaderFormatDirective,
             aInCommentFormatDirective,
             aInIsUserFile,
            aInItem);
    }

    /**
     * Optional API required by framework to regulate triggering of tasks.
     * This method identifies whether this task should be triggered for the item passed in.
     * @param aIn item for which task is considered to be trriggered
     * @return true if task shouold be triggered, false if task should be skipped for this item.
     */
    public static boolean shouldTriggerTask(Item aIn)
    {
        return ((MClass)aIn).isConcrete();
    }

    /**
     * Optional API required by framework to identify namespace/module-space for the corresponding generated file.
     * @param aIn item for which task is being triggered
     * @return name of the module/namespace that corresponds to the item
     */
    public static String getTargetModule(Item aIn)
    {
        return ((MClass) aIn).getModule().getLID().getName();
    }

    /**
     * Optional API required by the framework for transformation of file naming rule for the corresponding
     * generated file. This method can customize the location for the generated file.
     * @param aInFnr file name rule template
     * @param aInItem item for which file is generated
     * @return transformed file name rule
     */
    public static FileNameRule transformFileNameRule(FileNameRule aInFnr,Item aInItem)
    {
        String lTargetModue = getTargetModule(aInItem);
        String lOldRelativePath = aInFnr.getRelativePath();
        String lNewRelativePath = lOldRelativePath + "/include/" + Config.getProjName() + "/" + lTargetModue;
        return new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                getClassName((MClass)aInItem, false));
    }

    public void generate()
    {
        MClass lClass = (MClass) getItem();
        generate(0, lClass);
    }

    public static String getClassName(MClass aInClass, boolean aInFullyQualified)
    {
        return aInFullyQualified ? (getNamespace(aInClass,true) + "::" + aInClass.getLID().getName()) : aInClass.getLID().getName();
    }

    public static String getInclTag(MClass aInClass)
    {
        return "GI_" + aInClass.getModule().getLID().getName().toUpperCase() + '_' + aInClass.getLID().getName().toUpperCase() + "_HPP";
    }

    public static String getNamespace(String aInModuleName, boolean aInFullyQualified)
    {
        return aInFullyQualified ? (Config.getProjName() + "::" + aInModuleName) : (aInModuleName);
    }

    public static String getNamespace(MClass aInClass, boolean aInFullyQualified)
    {
        return getNamespace(aInClass.getModule().getLID().getName(), aInFullyQualified);
    }

    public static String getInclude(String aInPath, boolean aInIsBuiltIn)
    {
        return  "#include " + (aInIsBuiltIn ? '<' : '\"') + aInPath + (aInIsBuiltIn ? '>' : '\"');
    }

    public static String getIncludePath(MClass aIn)
    {
        return Config.getProjName() + "/" + getNamespace(aIn,false) + "/" + aIn.getLID().getName();
    }

    public static String getInclude(MClass aIn)
    {
        return getInclude(getIncludePath(aIn) + ".hpp", false);
    }
    private void generate(int aInIndent, MClass aInClass)
    {
        String lInclTag = getInclTag(aInClass);
        out.println(aInIndent,"#pragma once");
        out.println(aInIndent,"#ifndef " + lInclTag);
        out.println(aInIndent,"#define " + lInclTag);
        genIncludes(aInIndent, aInClass);
        genBody(aInIndent, aInClass);
        out.println(aInIndent,"#endif // " + lInclTag);

    }

    private void genIncludes(int aInIndent, MClass aInClass)
    {
        out.println();
        out.println(aInIndent,getInclude("boost/optional.hpp", true));
        out.println(aInIndent,getInclude("opflex/modb/URIBuilder.h", false));
        out.println(aInIndent,getInclude("opflex/modb/mo-internal/MO.h", false));
        TreeMap<Ident, MClass> lConts = new TreeMap<>();
        aInClass.getContainsClasses(lConts);
        for (MClass lThis : lConts.values())
        {
            out.printIncodeComment(aInIndent, "contains: " + lThis);
            out.println(aInIndent, getInclude(lThis), false);
        }
    }

    private void genBody(int aInIndent, MClass aInClass)
    {
        out.println();
        String lNs = getNamespace(aInClass, false);

        out.println(aInIndent, "namespace " + Config.getProjName() + " {");
        out.println(aInIndent, "namespace " + lNs + " {");
        out.println();
        genClass(aInIndent, aInClass);
        out.println();
        out.println(aInIndent, "} // namespace " + lNs);
        out.println(aInIndent, "} // namespace " + Config.getProjName());
    }

    private void genClass(int aInIndent, MClass aInClass)
    {
        out.println(aInIndent, "/** " + aInClass.getLID().getName() + " */");
        String lInherit;
        MClass lSuperclass = aInClass.getSuperclass();
        if (lSuperclass != null && lSuperclass.isConcrete())
        {
            lInherit = " : public " + Config.getProjName() + "::" +
                getNamespace(lSuperclass, false) + "::" + lSuperclass.getLID().getName();
        }
        else
        {
            lInherit = " : public opflex::modb::mointernal::MO";
        }
        out.println(aInIndent, "class " + aInClass.getLID().getName() + lInherit);
        out.println(aInIndent, "{");
        genPublic(aInIndent + 1, aInClass);
        out.println(aInIndent, "}; // class " + aInClass.getLID().getName());
    }

    private void genPublic(int aInIndent, MClass aInClass)
    {
        out.println(aInIndent - 1, "public:");
        out.println();
        genClassId(aInIndent, aInClass);
        genProps(aInIndent, aInClass);
        genResolvers(aInIndent, aInClass);
        genRemove(aInIndent, aInClass);
        genListenerReg(aInIndent, aInClass);
        genConstructor(aInIndent, aInClass);
    }

    private void genClassId(int aInIndent, MClass aInClass)
    {
        String lClassName = getClassName(aInClass, false);
        out.printHeaderComment(aInIndent, Collections.singletonList("The unique class ID for " + lClassName));
        out.println(aInIndent, "static const opflex::modb::class_id_t CLASS_ID = " + aInClass.getGID().getId() + ";");
        out.println();
    }

    private void genProps(int aInIndent, MClass aInClass)
    {
        TreeMap<String, MProp> lProps = new TreeMap<>();
        aInClass.findProp(lProps, true); // false
        for (MProp lProp : lProps.values())
        {
            genProp(aInIndent, aInClass, lProp, lProp.getPropId(aInClass));
        }
    }

    public static String getTypeAccessor(String aInPType)
    {
        if (aInPType.startsWith("Enum") ||
            aInPType.startsWith("Bitmask") ||
            aInPType.startsWith("UInt"))
        {
            aInPType = "UInt64";
        }
        else if (aInPType.equals("URI") ||
                 aInPType.equals("IP") ||
                 aInPType.equals("UUID"))
        {
            aInPType = "String";
        }
        return aInPType;
    }

    public static String getCast(String aInPType, String aInSyntax)
    {
        if (aInPType.startsWith("Enum") ||
            aInPType.startsWith("Bitmask") ||
            (aInPType.startsWith("UInt") &&
             !aInPType.equals("UInt64")))
        {
            return "(" + aInSyntax + ")";
        }
        return "";
    }

    private String toUnsignedStr(int aInInt) {
    	return Long.toString(aInInt & 0xFFFFFFFFL) + "ul";
    }

    private void genProp(int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx)
    {
        MProp lBaseProp = aInProp.getBase();
        MType lType = lBaseProp.getType(false);
        MType lBaseType = lType.getBuiltInType();

        LinkedList<String> lComments = new LinkedList<>();
        aInProp.getComments(lComments);

        if (aInClass.isConcreteSuperclassOf("relator/Source") &&
            aInProp.getLID().getName().toLowerCase().startsWith("target"))
        {
            if (aInProp.getLID().getName().equalsIgnoreCase("targetName"))
            {
                genRef(aInIndent,aInClass, aInPropIdx, lComments);
            }
        }
        else
        {
            genPropCheck(aInIndent, aInProp,aInPropIdx, lBaseType,lComments);
            genPropAccessor(aInIndent, aInProp, aInPropIdx, lBaseType, lComments);
            genPropDefaultedAccessor(aInIndent, aInProp, lBaseType, lComments);
            genPropMutator(aInIndent, aInClass, aInProp, aInPropIdx, lBaseType, lComments);
            genPropUnset(aInIndent, aInClass, aInProp, aInPropIdx, lBaseType, lComments);
        }
    }

    private void genRef(
        int aInIndent, MClass aInClass, int aInPropIdx,
        Collection<String> aInComments)
    {
        genRefCheck(aInIndent, aInPropIdx, aInComments);
        genRefAccessors(aInIndent, aInPropIdx, aInComments);
        genRefMutators(aInIndent, aInClass, aInPropIdx, aInComments);
        genRefUnset(aInIndent, aInClass, aInPropIdx, aInComments);
    }
    
    private void genPropCheck(
        int aInIndent, int aInPropIdx, Collection<String> aInComments,
        String aInCheckName, String aInPType)
    {
        //
        // COMMENT
        //
        int lCommentSize = 2 + aInComments.size();
        String[] lComment = new String[lCommentSize];
        int lCommentIdx = 0;
        lComment[lCommentIdx++] = "Check whether " + aInCheckName + " has been set";
        for (String lCommLine : aInComments)
        {
            lComment[lCommentIdx++] = lCommLine;
        }
        lComment[lCommentIdx++] = "@return true if " + aInCheckName + " has been set";
        out.printHeaderComment(aInIndent,lComment);
        //
        // METHOD DEFINITION
        //
        out.println(aInIndent,"virtual bool is" + Strings.upFirstLetter(aInCheckName) + "Set() const");

        //
        // METHOD BODY
        //
        out.println(aInIndent,"{");
            out.println(aInIndent + 1, "return getObjectInstance().isSet(" + toUnsignedStr(aInPropIdx) +
                                       ", opflex::modb::PropertyInfo::" + aInPType + ");");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropCheck(
        int aInIndent, MProp aInProp, int aInPropIdx, MType aInBaseType, Collection<String> aInComments)
    {
        String lPType = FMetaDef.getTypeName(aInBaseType);
        genPropCheck(aInIndent, aInPropIdx,
                aInComments, aInProp.getLID().getName(),
                     lPType);
    }

    private void genRefCheck(
        int aInIndent, int aInPropIdx, Collection<String> aInComments)
    {
        genPropCheck(aInIndent, aInPropIdx,
                aInComments, "target", "REFERENCE");
    }

    private void genPropAccessor(
        int aInIndent, int aInPropIdx, Collection<String> aInComments, String aInCheckName,
        String aInName, String aInEffSyntax, String aInPType, String aInCast, String aInAccessor)
    {
        //
        // COMMENT
        //
        int lCommentSize = 2 + aInComments.size();
        String[] lComment = new String[lCommentSize];
        int lCommentIdx = 0;
        lComment[lCommentIdx++] = "Get the value of " + aInName + " if it has been set.";

        for (String lCommLine : aInComments)
        {
            lComment[lCommentIdx++] = lCommLine;
        }
        lComment[lCommentIdx++] = "@return the value of " + aInName + " or boost::none if not set";
        out.printHeaderComment(aInIndent,lComment);
        out.println(aInIndent,"virtual boost::optional<" + aInEffSyntax + "> get" + Strings.upFirstLetter(aInName) + "() const");
        out.println(aInIndent,"{");
        out.println(aInIndent + 1,"if (is" + Strings.upFirstLetter(aInCheckName) + "Set())");
        out.println(aInIndent + 2,"return " + aInCast + "getObjectInstance().get" + aInPType + "(" + toUnsignedStr(aInPropIdx) + ")" + aInAccessor + ";");
        out.println(aInIndent + 1,"return boost::none;");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropAccessor(
        int aInIndent, MProp aInProp, int aInPropIdx, MType aInBaseType,
        Collection<String> aInComments)
    {
        String lName = aInProp.getLID().getName();
        String lPType = Strings.upFirstLetter(aInBaseType.getLID().getName());
        String lEffSyntax = getPropEffSyntax(aInBaseType);
        String lCast = getCast(lPType, lEffSyntax);
        lPType = getTypeAccessor(lPType);
        genPropAccessor(aInIndent, aInPropIdx, aInComments,
                        lName, lName, lEffSyntax, lPType, lCast, "");
    }

    private void genRefAccessors(
        int aInIndent, int aInPropIdx,
        Collection<String> aInComments)
    {
        String lName = "target";
        genPropAccessor(aInIndent, aInPropIdx, aInComments,
                        lName, lName + "Class", "opflex::modb::class_id_t", "Reference", "", ".first");
        genPropAccessor(aInIndent, aInPropIdx, aInComments,
                        lName, lName + "URI", "opflex::modb::URI", "Reference", "", ".second");
    }

    private void genPropDefaultedAccessor(
        int aInIndent, Collection<String> aInComments, String aInName, String aInEffSyntax)
    {
        //
        // COMMENT
        //
        int lCommentSize = 3 + aInComments.size();
        String[] lComment = new String[lCommentSize];
        int lCommentIdx = 0;
        lComment[lCommentIdx++] = "Get the value of " + aInName + " if set, otherwise the value of default passed in.";

        for (String lCommLine : aInComments)
        {
            lComment[lCommentIdx++] = lCommLine;
        }
        //
        // DEF
        //
        lComment[lCommentIdx++] = "@param defaultValue default value returned if the property is not set";
        lComment[lCommentIdx++] = "@return the value of " + aInName + " if set, otherwise the value of default passed in";
        out.printHeaderComment(aInIndent,lComment);
        out.println(aInIndent, "virtual " + aInEffSyntax + " get" + Strings.upFirstLetter(aInName) + "(" + aInEffSyntax + " defaultValue) const");
        //
        // BODY
        //
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "return get" + Strings.upFirstLetter(aInName) + "().get_value_or(defaultValue);");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropDefaultedAccessor(
        int aInIndent, MProp aInProp, MType aInBaseType,
        Collection<String> aInComments)
    {
        genPropDefaultedAccessor(aInIndent, aInComments,
                                 aInProp.getLID().getName(),
                                 getPropEffSyntax(aInBaseType));
    }

    private void genPropMutator(
        int aInIndent, MClass aInClass, int aInPropIdx, Collection<String> aInBaseComments,
        Collection<String> aInComments, String aInName, String aInPType, String aInEffSyntax,
        String aInParamName, String aInParamHelp, String aInSetterPrefix)
    {
        //
        // COMMENT
        //
        ArrayList<String> lComment = new ArrayList<>(aInBaseComments);
        if (aInComments.size() > 0) lComment.add("");
        lComment.addAll(aInComments);
        lComment.add("");
        lComment.add("@param " + aInParamName + " " + aInParamHelp);
        lComment.add("@return a reference to the current object");
        lComment.add("@throws std::logic_error if no mutator is active");
        lComment.add("@see opflex::modb::Mutator");
        out.printHeaderComment(aInIndent,lComment);
        //
        // DEF
        //
        out.println(aInIndent, "virtual " + getClassName(aInClass, true) + "& set" + Strings.upFirstLetter(aInName) + "(" + aInEffSyntax + " " + aInParamName + ")");
        //
        // BODY
        //
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "getTLMutator().modify(getClassId(), getURI())->set" + aInPType + "(" + toUnsignedStr(aInPropIdx) + aInSetterPrefix + ", " + aInParamName + ");");
        out.println(aInIndent + 1, "return *this;");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genNamedPropMutators(
        int aInIndent, MClass aInClass, MClass aInRefClass, int aInPropIdx,
        List<Pair<String, MNameRule>> aInNamingPath, boolean aInIsUniqueNaming,
        String aInMethName, String aInPType, String aInSetterPrefix)
    {
        String lRefClassName = getClassName(aInRefClass, false);
        ArrayList<String> lComment = new ArrayList<>(Arrays.asList(
            "Set the reference to point to an instance of " + lRefClassName,
            "in the currently-active mutator by constructing its URI from the",
            "path elements that lead to it.",
            "",
            "The reference URI generated by this function will take the form:",
            getUriDoc(aInNamingPath),
            ""));
        addPathComment(aInNamingPath, lComment);
        lComment.add("");
        lComment.add("@throws std::logic_error if no mutator is active");
        lComment.add("@return a reference to the current object");
        lComment.add("@see opflex::modb::Mutator");
        out.printHeaderComment(aInIndent,lComment);

        //
        // DEF
        //
        String lMethName = getMethName(aInNamingPath, aInIsUniqueNaming, aInMethName);
        out.println(aInIndent,  getClassName(aInClass, true) + "& set" + Strings.upFirstLetter(lMethName) + "(");
        boolean lIsFirst = true;
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());

            Collection<MNameComponent> lNcs = lNr.getComponents();
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    if (lIsFirst)
                    {
                        lIsFirst = false;
                    }
                    else
                    {
                        out.println(",");
                    }
                    out.print(aInIndent + 1, getPropParamDef(lThisContClass, lNc.getPropName()));
                }
            }
        }
        if (lIsFirst)
        {
            out.println(aInIndent + 1, ")");
        }
        else
        {
            out.println(")");
        }
        //
        // BODY
        //
        String lUriBuilder = getUriBuilder(aInNamingPath);
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "getTLMutator().modify(getClassId(), getURI())->set" + aInPType + "(" + toUnsignedStr(aInPropIdx) + aInSetterPrefix + ", " + lUriBuilder + ");");
        out.println(aInIndent + 1, "return *this;");
        out.println(aInIndent,"}");
        out.println();
    }
    
    private void genPropMutator(
        int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInBaseType,
        Collection<String> aInComments)
    {
        String lName = aInProp.getLID().getName();
        String lPType = Strings.upFirstLetter(aInBaseType.getLID().getName());
        lPType = getTypeAccessor(lPType);
        List<String> lComments = Collections.singletonList(
            "Set " + lName + " to the specified value in the currently-active mutator.");
        genPropMutator(aInIndent, aInClass, aInPropIdx,
                lComments, aInComments, lName, lPType,
                       getPropEffSyntax(aInBaseType), "newValue", "the new value to set.",
                       "");
    }

    private void genRefMutators(
        int aInIndent, MClass aInClass, int aInPropIdx, Collection<String> aInComments)
    {
        for (MClass lTargetClass : ((MRelationshipClass) aInClass).getTargetClasses(true))
        {
            String lName = "target" +
                    Strings.upFirstLetter(lTargetClass.getLID().getName());
            List<String> lComments = Arrays.asList(
                "Set the reference to point to an instance of " + getClassName(lTargetClass, false),
                "with the specified URI");
            genPropMutator(aInIndent, aInClass, aInPropIdx,
                    lComments, aInComments, lName, "Reference",
                           "const opflex::modb::URI&", "uri", "The URI of the reference to add",
                           ", " + lTargetClass.getGID().getId());

            Collection<List<Pair<String, MNameRule>>> lNamingPaths = new LinkedList<>();
            boolean lIsUniqueNaming = lTargetClass.getNamingPaths(lNamingPaths, Language.CPP);
            for (List<Pair<String, MNameRule>> lNamingPath : lNamingPaths)
            {
                genNamedPropMutators(aInIndent, aInClass, lTargetClass, aInPropIdx, lNamingPath, lIsUniqueNaming,
                                    lName, "Reference", ", " + lTargetClass.getGID().getId());
            }
        }
    }

    private void genPropUnset(
        int aInIndent, MClass aInClass, int aInPropIdx,
        Collection<String> aInComments, String aInName, String aInPType)
    {
        //
        // COMMENT
        //
        int lCommentSize = 4 + aInComments.size();
        String[] lComment = new String[lCommentSize];
        int lCommentIdx = 0;
        lComment[lCommentIdx++] = "Unset " + aInName + " in the currently-active mutator.";

        for (String lCommLine : aInComments)
        {
            lComment[lCommentIdx++] = lCommLine;
        }
        lComment[lCommentIdx++] = "@throws std::logic_error if no mutator is active";
        lComment[lCommentIdx++] = "@return a reference to the current object";
        lComment[lCommentIdx++] = "@see opflex::modb::Mutator";
        out.printHeaderComment(aInIndent,lComment);

        out.println(aInIndent,  "virtual " + getClassName(aInClass, true) + "& unset" + Strings.upFirstLetter(aInName) + "()");
        //
        // BODY
        //
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "getTLMutator().modify(getClassId(), getURI())->unset(" + toUnsignedStr(aInPropIdx) + ", " +
                                   "opflex::modb::PropertyInfo::" + aInPType + ", " +
                                   "opflex::modb::PropertyInfo::SCALAR);");
        out.println(aInIndent + 1, "return *this;");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropUnset(
        int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInBaseType,
        Collection<String> aInComments)
    {
        genPropUnset(aInIndent, aInClass, aInPropIdx, aInComments,
                     aInProp.getLID().getName(), FMetaDef.getTypeName(aInBaseType));
    }

    private void genRefUnset(
        int aInIndent, MClass aInClass, int aInPropIdx,
        Collection<String> aInComments)
    {
        genPropUnset(aInIndent, aInClass, aInPropIdx, aInComments,
                     "target", "REFERENCE");
    }

    private void genResolvers(int aInIdent, MClass aInClass)
    {
        if (aInClass.isRoot())
        {
            genRootCreation(aInIdent,aInClass);
        }
        if (aInClass.isConcrete())
        {
            genSelfResolvers(aInIdent, aInClass);
        }
        genChildrenResolvers(aInIdent, aInClass);
    }

    private void genRootCreation(int aInIdent, MClass aInClass)
    {
        String lClassName = getClassName(aInClass, true);
        ArrayList<String> lComment = new ArrayList<>(Arrays.asList(
            "Create an instance of " + getClassName(aInClass, false) + ", the root element in the",
            "management information tree, for the given framework instance in",
            "the currently-active mutator.",
            "",
            "@param framework the framework instance to use",
            "@throws std::logic_error if no mutator is active",
            "@see opflex::modb::Mutator"));
        out.printHeaderComment(aInIdent,lComment);

        out.println(aInIdent, "static std::shared_ptr<" + lClassName + "> createRootElement(opflex::ofcore::OFFramework& framework)");
        out.println(aInIdent,"{");
        out.println(aInIdent + 1, "return opflex::modb::mointernal::MO::createRootElement<" + lClassName + ">(framework, CLASS_ID);");
        out.println(aInIdent, "}");
        out.println();
    }

    private void genChildrenResolvers(int aInIdent, MClass aInClass)
    {
        Map<Ident,MClass> lConts = new TreeMap<>();
        aInClass.getContainsClasses(lConts);
        for (MClass lChildClass : lConts.values())
        {
            genChildResolvers(aInIdent,aInClass,lChildClass);
        }
    }

    private void genSelfResolvers(int aInIdent, MClass aInClass)
    {
        String lFullyQualifiedClassName = getClassName(aInClass, true);

        String lclassName = getClassName(aInClass, false);
        String[] lComment = 
            {"Retrieve an instance of " + lclassName + " from the managed",
             "object store.  If the object does not exist in the local store,",
             "returns boost::none.  Note that even though it may not exist",
             "locally, it may still exist remotely.",
             "",
             "@param framework the framework instance to use",
             "@param uri the URI of the object to retrieve",
             "@return a shared pointer to the object or boost::none if it",
             "does not exist."};
        out.printHeaderComment(aInIdent,lComment);

        out.println(aInIdent, "static boost::optional<std::shared_ptr<" + lFullyQualifiedClassName + "> > resolve(");
        out.println(aInIdent + 1, "opflex::ofcore::OFFramework& framework,");
        out.println(aInIdent + 1, "const opflex::modb::URI& uri)");
        out.println(aInIdent, "{");
        out.println(aInIdent + 1, "return opflex::modb::mointernal::MO::resolve<" + lFullyQualifiedClassName + ">(framework, CLASS_ID, uri);");
        out.println(aInIdent, "}");
        out.println();

        Collection<List<Pair<String, MNameRule>>> lNamingPaths = new LinkedList<>();
        boolean lIsUniqueNaming = aInClass.getNamingPaths(lNamingPaths, Language.CPP);
        if (lIsUniqueNaming)
        {
            for (List<Pair<String, MNameRule>> lNamingPath : lNamingPaths)
            {
                genNamedSelfResolvers(aInIdent, aInClass, lNamingPath);
            }
        }
    }

    private void genRemove(int aInIdent, MClass aInClass)
    {
        String lclassName = getClassName(aInClass, false);
        String[] lComment = 
            {"Remove this instance using the currently-active mutator.  If",
             "the object does not exist, then this will be a no-op.  If this",
             "object has any children, they will be garbage-collected at some",
             "future time.",
             "",
             "@throws std::logic_error if no mutator is active"};
        out.printHeaderComment(aInIdent,lComment);
        out.println(aInIdent, "virtual void remove()");
        out.println(aInIdent, "{");
        out.println(aInIdent + 1, "getTLMutator().remove(CLASS_ID, getURI());");
        out.println(aInIdent, "}");
        out.println();

        String[] lComment2 = 
            {"Remove the " + lclassName + " object with the specified URI",
             "using the currently-active mutator.  If the object does not exist,",
             "then this will be a no-op.  If this object has any children, they",
             "will be garbage-collected at some future time.",
             "",
             "@param framework the framework instance to use",
             "@param uri the URI of the object to remove",
             "@throws std::logic_error if no mutator is active"};
        out.printHeaderComment(aInIdent,lComment2);
        out.println(aInIdent, "static void remove(opflex::ofcore::OFFramework& framework,");
        out.println(aInIdent, "                   const opflex::modb::URI& uri)");
        out.println(aInIdent, "{");
        out.println(aInIdent + 1, "MO::remove(framework, CLASS_ID, uri);");
        out.println(aInIdent, "}");
        out.println();

        Collection<List<Pair<String, MNameRule>>> lNamingPaths = new LinkedList<>();
        boolean lIsUniqueNaming = aInClass.getNamingPaths(lNamingPaths, Language.CPP);
        for (List<Pair<String, MNameRule>> lNamingPath : lNamingPaths)
        {
            if (!hasValidPath(lNamingPath)) continue;
            genNamedSelfRemovers(aInIdent, aInClass, lNamingPath, lIsUniqueNaming);
        }
    }

    private boolean hasValidPath(List<Pair<String, MNameRule>> aInNamingPath) {
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();

            Collection<MNameComponent> lNcs = lNr.getComponents();
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    return true;
                }
            }
        }
        return false;
    }

    private void addPathComment(MClass aInThisContClass,
                                MClass aInTargetClass,
                                Collection<MNameComponent> aInNcs,
                                List<String> result)
    {
        String lclassName = getClassName(aInThisContClass, false);
        for (MNameComponent lNc : aInNcs)
        {
            if (lNc.hasPropName() && (aInTargetClass == null || !lNc.getPropName().equalsIgnoreCase("targetClass")))
            {
                result.add("@param " + 
                           getPropParamName(aInThisContClass, lNc.getPropName()) +
                           " the value of " +
                           getPropParamName(aInThisContClass, lNc.getPropName()) + ",");
                result.add("a naming property for " + lclassName);
            }
        }
    }

    private void addPathComment(List<Pair<String, MNameRule>> aInNamingPath,
                                List<String> result)
    {
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());

            Collection<MNameComponent> lNcs = lNr.getComponents();
            addPathComment(lThisContClass, null, lNcs, result);
        }
    }
    
    private void genNamedSelfRemovers(int aInIdent, MClass aInClass, 
                                      List<Pair<String, MNameRule>> aInNamingPath, 
                                      boolean aInIsUniqueNaming)
    {
        String lclassName = getClassName(aInClass, false);
        String lMethodName = getRemoverMethName(aInNamingPath, aInIsUniqueNaming);

        ArrayList<String> comment = new ArrayList<>(Arrays.asList(
            "Remove the " + lclassName + " object with the specified path",
            "elements from the managed object store.  If the object does",
            "not exist, then this will be a no-op.  If this object has any",
            "children, they will be garbage-collected at some future time.",
            "",
            "The object URI generated by this function will take the form:",
            getUriDoc(aInNamingPath),
            "",
            "@param framework the framework instance to use"));
        addPathComment(aInNamingPath, comment);
        comment.add("@throws std::logic_error if no mutator is active");
        out.printHeaderComment(aInIdent,comment);

        out.println(aInIdent,"static void " + lMethodName + "(");
        out.print(aInIdent + 1, "opflex::ofcore::OFFramework& framework");
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());

            Collection<MNameComponent> lNcs = lNr.getComponents();
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    out.println(",");
                    out.print(aInIdent + 1, getPropParamDef(lThisContClass, lNc.getPropName()));
                }
            }
        }
        out.println(")");
        out.println(aInIdent,"{");
        out.println(aInIdent + 1, "MO::remove(framework, CLASS_ID, " + getUriBuilder(aInNamingPath) + ");");
        out.println(aInIdent,"}");
        out.println();
    }

    private static String getMethName(List<Pair<String, MNameRule>> aInNamingPath,
                                      boolean aInIsUniqueNaming,
                                      String prefix)
    {
        if (aInIsUniqueNaming)
        {
            return prefix;
        }
        else
        {
            StringBuilder lSb = new StringBuilder();
            lSb.append(prefix).append("Under");
            int lSize = aInNamingPath.size();
            for (Pair<String, MNameRule> lPathNode : aInNamingPath)
            {
                if (0 < --lSize)
                {
                    MClass lClass = MClass.get(lPathNode.getFirst());
                    Module lMod = lClass.getModule();
                    lSb.append(Strings.upFirstLetter(lMod.getLID().getName()));
                    lSb.append(Strings.upFirstLetter(lClass.getLID().getName()));
                }
            }
            return lSb.toString();
        }
    }
    
    private static String getResolverMethName(List<Pair<String, MNameRule>> aInNamingPath)
    {
        return getMethName(aInNamingPath, true, "resolve");
    }

    private static String getRemoverMethName(List<Pair<String, MNameRule>> aInNamingPath,
                                             boolean aInIsUniqueNaming)
    {
        return getMethName(aInNamingPath, aInIsUniqueNaming, "remove");
    }

    public static void getPropParamName(MClass aInClass, String aInPropName, StringBuilder aOutSb)
    {
        aOutSb.append(aInClass.getModule().getLID().getName());
        aOutSb.append(Strings.upFirstLetter(aInClass.getLID().getName()));
        aOutSb.append(Strings.upFirstLetter(aInPropName));
    }

    public static String getPropParamName(MClass aInClass, String aInPropName)
    {
        StringBuilder lSb = new StringBuilder();
        getPropParamName(aInClass,aInPropName,lSb);
        return lSb.toString();
    }

    public static String getPropEffSyntax(MType aInBaseType)
    {
        StringBuilder lRet = new StringBuilder();
        MLanguageBinding lLang = aInBaseType.getLanguageBinding(Language.CPP);
        boolean lPassAsConst = lLang.getPassConst();
        PassBy lPassBy = lLang.getPassBy();

        String lSyntax = aInBaseType.getLanguageBinding(Language.CPP).getSyntax();
        if (lPassAsConst)
        {
            lRet.append("const ");
        }
        lRet.append(lSyntax);

        switch (lPassBy)
        {
        case REFERENCE:
        case POINTER:

            lRet.append('&');
            break;

        case VALUE:
        default:

            break;
        }
        return lRet.toString();
    }

    public static String getPropParamDef(MClass aInClass, String aInPropName)
    {
        MProp lProp = aInClass.findProp(aInPropName, false);
        if (null == lProp)
        {
            Severity.DEATH.report(aInClass.toString(),
                                  "preparing param defs for prop: " + aInPropName,
                                  "no such property: " + aInPropName, "");
        }
        MProp lBaseProp = lProp.getBase();
        MType lType = lBaseProp.getType(false);
        MType lBaseType = lType.getBuiltInType();
        StringBuilder lRet = new StringBuilder();
        lRet.append(getPropEffSyntax(lBaseType));
        lRet.append(" ");
        getPropParamName(aInClass, aInPropName, lRet);
        return lRet.toString();
    }

    public static String getUriBuilder(List<Pair<String, MNameRule>> aInNamingPath)
    {
        StringBuilder lSb = new StringBuilder();
        getUriBuilder(aInNamingPath, lSb);
        return lSb.toString();
    }

    public static void getUriBuilder(List<Pair<String, MNameRule>> aInNamingPath, StringBuilder aOut)
    {
        aOut.append("opflex::modb::URIBuilder()");
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());
            Collection<MNameComponent> lNcs = lNr.getComponents();
            aOut.append(".addElement(\"");

            aOut.append(lThisContClass.getFullConcatenatedName());
            aOut.append("\")");
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    aOut.append(".addElement(");
                    getPropParamName(lThisContClass, lNc.getPropName(), aOut);
                    aOut.append(")");
                }
            }
        }
        aOut.append(".build()");
    }

    public static String getUriDoc(List<Pair<String, MNameRule>> aInNamingPath)
    {
        StringBuilder lSb = new StringBuilder();
        getUriDoc(aInNamingPath, lSb);
        return lSb.toString();
    }
    public static void getUriDoc(List<Pair<String, MNameRule>> aInNamingPath, StringBuilder aOut)
    {
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());
            Collection<MNameComponent> lNcs = lNr.getComponents();
            aOut.append('/').append(lThisContClass.getFullConcatenatedName());
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    aOut.append("/[");
                    getPropParamName(lThisContClass, lNc.getPropName(), aOut);
                    aOut.append("]");
                }
            }
        }
    }

    public static String getUriBuilder(MClass aInChildClass, MNameRule aInNamingRule)
    {
        StringBuilder lSb = new StringBuilder();
        getUriBuilder(aInChildClass, aInNamingRule, lSb);
        return lSb.toString();

    }
    public static void getUriBuilder(MClass aInChildClass, MNameRule aInNamingRule, StringBuilder aOut)
    {
        aOut.append("opflex::modb::URIBuilder(getURI())");
        aOut.append(".addElement(\"");
        aOut.append(aInChildClass.getFullConcatenatedName());
        aOut.append("\")");
        Collection<MNameComponent> lNcs = aInNamingRule.getComponents();
        for (MNameComponent lNc : lNcs)
        {
            if (lNc.hasPropName())
            {
                aOut.append(".addElement(");
                getPropParamName(aInChildClass, lNc.getPropName(), aOut);
                aOut.append(")");
            }
        }
        aOut.append(".build()");
    }

    private void genNamedSelfResolvers(int aInIdent, MClass aInClass, List<Pair<String, MNameRule>> aInNamingPath)
    {
        String lClassName = getClassName(aInClass, false);
        String lMethodName = getResolverMethName(aInNamingPath);

        ArrayList<String> comment = new ArrayList<>(Arrays.asList(
            "Retrieve an instance of " + lClassName + " from the managed",
            "object store by constructing its URI from the path elements",
            "that lead to it.  If the object does not exist in the local",
            "store, returns boost::none.  Note that even though it may not",
            "exist locally, it may still exist remotely.",
            "",
            "The object URI generated by this function will take the form:",
            getUriDoc(aInNamingPath),
            "",
            "@param framework the framework instance to use "));
        addPathComment(aInNamingPath, comment);
        comment.add("@return a shared pointer to the object or boost::none if it");
        comment.add("does not exist.");
        out.printHeaderComment(aInIdent,comment);

        out.println(aInIdent,"static boost::optional<std::shared_ptr<" + getClassName(aInClass,true)+ "> > " + lMethodName + "(");
        out.print(aInIdent + 1, "opflex::ofcore::OFFramework& framework");
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());

            Collection<MNameComponent> lNcs = lNr.getComponents();
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    out.println(",");
                    out.print(aInIdent + 1, getPropParamDef(lThisContClass, lNc.getPropName()));
                }
            }
        }
        out.println(")");
        out.println(aInIdent,"{");
        out.println(aInIdent + 1, "return resolve(framework," + getUriBuilder(aInNamingPath) + ");");
        out.println(aInIdent,"}");
        out.println();
    }

    private void genChildAdder(int aInIdent, MClass aInParentClass, MClass aInChildClass,
                               Collection<MNameComponent> aInNcs,
                               String aInFormattedChildClassName,
                               String aInConcatenatedChildClassName,
                               String aInUriBuilder,
                               MNameRule aInChildNr,
                               boolean aInMultipleChildren,
                               MClass aInTargetClass,
                               boolean aInTargetUnique)
    {

        ArrayList<String> comment = new ArrayList<>(Arrays.asList(
            "Create a new child object with the specified naming properties",
            "and make it a child of this object in the currently-active",
            "mutator.  If the object already exists in the store, get a",
            "mutable copy of that object.  If the object already exists in",
            "the mutator, get a reference to the object.",
            ""));
        addPathComment(aInChildClass, aInTargetClass, aInNcs, comment);
        comment.add("@throws std::logic_error if no mutator is active");
        comment.add("@return a shared pointer to the (possibly new) object");
        out.printHeaderComment(aInIdent,comment);
        String lTargetClassName = null;
        if (aInTargetClass != null)
            lTargetClassName = Strings.upFirstLetter(aInTargetClass.getLID().getName());
        String lMethodName = "add" + aInConcatenatedChildClassName;

        if (!aInTargetUnique && aInMultipleChildren && aInTargetClass != null)
        {
            lMethodName += lTargetClassName;
        }
        out.println(aInIdent, "std::shared_ptr<" +  aInFormattedChildClassName + "> " + lMethodName + "(");

        boolean lIsFirst = true;
        MNameComponent lClassProp = null;
        for (MNameComponent lNc : aInNcs)
        {
            if (lNc.hasPropName())
            {
                if (aInTargetClass != null)
                {
                    if (lNc.getPropName().equalsIgnoreCase("targetClass"))
                    {
                        lClassProp = lNc;
                        continue;
                    }
                }
                if (lIsFirst)
                {
                    lIsFirst = false;
                }
                else
                {
                    out.println(",");
                }
                out.print(aInIdent + 1, getPropParamDef(aInChildClass, lNc.getPropName()));
            }
        }
        if (lIsFirst)
        {
            out.println(aInIdent + 1, ")");
        }
        else
        {
            out.println(")");
        }
        out.println(aInIdent,"{");
        if (aInTargetClass != null && lClassProp != null)
        {
            String lValue = Strings.upFirstLetter(aInTargetClass.getModule().getLID().getName()) + Strings.upFirstLetter(aInTargetClass.getLID().getName());
            out.println(aInIdent + 1, "static const std::string " + getPropParamName(aInChildClass, lClassProp.getPropName()) + " = \"" + lValue + "\";");
        }
        out.println(aInIdent + 1, "std::shared_ptr<" + aInFormattedChildClassName + "> result = addChild<" + aInFormattedChildClassName+ ">(");
        out.println(aInIdent + 2, "CLASS_ID, getURI(), " + toUnsignedStr(aInChildClass.getClassAsPropId(aInParentClass)) + ", " + aInChildClass.getGID().getId() + ",");
        out.println(aInIdent + 2, aInUriBuilder);
        out.println(aInIdent + 2, ");");

        aInNcs = aInChildNr.getComponents();
        for (MNameComponent lNc : aInNcs)
        {
            if (lNc.hasPropName())
            {
                String lPropName = Strings.upFirstLetter(lNc.getPropName());
                String lArgs = getPropParamName(aInChildClass, lNc.getPropName());
                if (aInTargetClass != null) {
                    if (lPropName.equalsIgnoreCase("targetClass"))
                        continue;
                    if (lPropName.equalsIgnoreCase("targetName"))
                    {
                        lPropName = "Target" + lTargetClassName;
                        lArgs = "opflex::modb::URI(" + lArgs + ")";
                    }
                }
                out.println(aInIdent + 1,
                            "result->set" + lPropName +
                            "(" + lArgs + ");");
            }
        }

        out.println(aInIdent + 1, "return result;");
        out.println(aInIdent,"}");
        out.println();
    }
    
    private void genChildResolver(
        int aInIdent, MClass aInChildClass,
        Collection<MNameComponent> aInNcs,
        String aInFormattedChildClassName,
        String aInConcatenatedChildClassName,
        String aInUriBuilder,
        boolean aInMultipleChildren,
        MClass aInTargetClass,
        boolean aInTargetUnique)
    {

        ArrayList<String> comment = new ArrayList<>(Arrays.asList(
            "Retrieve the child object with the specified naming",
            "properties. If the object does not exist in the local store,",
            "returns boost::none.  Note that even though it may not exist",
            "locally, it may still exist remotely.",
            ""));
        addPathComment(aInChildClass, aInTargetClass, aInNcs, comment);
        comment.add("@return a shared pointer to the object or boost::none if it");
        comment.add("does not exist.");
        out.printHeaderComment(aInIdent,comment);
        
        String lTargetClassName = null;
        if (aInTargetClass != null)
            lTargetClassName = Strings.upFirstLetter(aInTargetClass.getLID().getName());
        String lMethodName = "resolve" + aInConcatenatedChildClassName;

        if (!aInTargetUnique && aInMultipleChildren && aInTargetClass != null)
        {
            lMethodName += lTargetClassName;
        }

        out.println(aInIdent, "boost::optional<std::shared_ptr<" +  aInFormattedChildClassName + "> > " + lMethodName + "(");

        boolean lIsFirst = true;
        MNameComponent lClassProp = null;
        for (MNameComponent lNc : aInNcs)
        {
            if (lNc.hasPropName())
            {
                if (aInTargetClass != null)
                {
                    if (lNc.getPropName().equalsIgnoreCase("targetClass"))
                    {
                        lClassProp = lNc;
                        continue;
                    }
                }
                if (lIsFirst)
                {
                    lIsFirst = false;
                }
                else
                {
                    out.println(",");
                }
                out.print(aInIdent + 1, getPropParamDef(aInChildClass, lNc.getPropName()));
            }
        }
        if (lIsFirst)
        {
            out.println(aInIdent + 1, ")");
        }
        else
        {
            out.println(")");
        }
        out.println(aInIdent,"{");
        if (aInTargetClass != null && lClassProp != null)
        {
            String lValue = Strings.upFirstLetter(aInTargetClass.getModule().getLID().getName()) + Strings.upFirstLetter(aInTargetClass.getLID().getName());
            out.println(aInIdent + 1, "static const std::string " + getPropParamName(aInChildClass, lClassProp.getPropName()) + " = \"" + lValue + "\";");
        }
        out.println(aInIdent + 1, "return " + aInFormattedChildClassName + "::resolve(getFramework(), " + aInUriBuilder + ");");
        out.println(aInIdent,"}");
        out.println();
    }

    private void genChildResolvers(int aInIdent, MClass aInParentClass, MClass aInChildClass)
    {
        MNamer lChildNamer = MNamer.get(aInChildClass.getGID().getName(),false);
        MNameRule lChildNr = lChildNamer.findNameRule(aInParentClass.getGID().getName());
        if (null != lChildNr)
        {
            String lFormattedChildClassName = getClassName(aInChildClass,true);
            String lConcatenatedChildClassName = aInChildClass.getFullConcatenatedName();
            String lUriBuilder = getUriBuilder(aInChildClass, lChildNr);
            Collection<MNameComponent> lNcs = lChildNr.getComponents();
            
            boolean lMultipleChildren = false;
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    lMultipleChildren = true;
                    break;
                }
            }
            
            if (aInChildClass.isConcreteSuperclassOf("relator/Source"))
                {
                    Collection<MClass> lTargetClasses = ((MRelationshipClass) aInChildClass).getTargetClasses(true);
                    for (MClass lTargetClass : lTargetClasses)
                    {
                        genChildResolver(aInIdent, aInChildClass, lNcs,
                                lFormattedChildClassName, lConcatenatedChildClassName,
                                lUriBuilder, lMultipleChildren, lTargetClass,
                                lTargetClasses.size() == 1);
                        genChildAdder(aInIdent, aInParentClass, aInChildClass, lNcs, 
                                lFormattedChildClassName, lConcatenatedChildClassName,
                                lUriBuilder, lChildNr, lMultipleChildren, lTargetClass,
                                lTargetClasses.size() == 1);
                        if (!lMultipleChildren) break;
                    }
                }
                else
                {
                    genChildResolver(aInIdent, aInChildClass, lNcs,
                            lFormattedChildClassName, lConcatenatedChildClassName,
                            lUriBuilder, lMultipleChildren, null, false);
                    genChildAdder(aInIdent, aInParentClass, aInChildClass, lNcs, 
                                  lFormattedChildClassName, lConcatenatedChildClassName,
                                  lUriBuilder, lChildNr, lMultipleChildren, null, false);
                }

            if (lMultipleChildren) {
                ArrayList<String> comment = new ArrayList<>(Arrays.asList(
                    "Resolve and retrieve all of the immediate children of type",
                    lFormattedChildClassName,
                    "",
                    "Note that this retrieves only those children that exist in the",
                    "local store.  It is possible that there are other children that",
                    "exist remotely.",
                    "",
                    "The resulting managed objects will be added to the result",
                    "vector provided.",
                    "",
                    "@param out a reference to a vector that will receive the child",
                    "objects."));
                out.printHeaderComment(aInIdent,comment);

                out.println(aInIdent,"void resolve" + lConcatenatedChildClassName + "(/* out */ std::vector<std::shared_ptr<" + lFormattedChildClassName+ "> >& out)");
                out.println(aInIdent,"{");
                out.println(aInIdent + 1, "opflex::modb::mointernal::MO::resolveChildren<" + lFormattedChildClassName + ">(");
                out.println(aInIdent + 2, "getFramework(), CLASS_ID, getURI(), " + toUnsignedStr(aInChildClass.getClassAsPropId(aInParentClass)) + ", " + aInChildClass.getGID().getId() + ", out);");
                out.println(aInIdent,"}");
                out.println();
            }
        }
        else
        {
            Severity.DEATH.report(aInParentClass.toString(), "child object resolver for " + aInChildClass.getGID().getName()," no naming rule", "");
        }
    }

    private void genListenerReg(int aInIndent, MClass aInClass)
    {
        if (aInClass.isConcrete())
        {
            out.printHeaderComment(aInIndent, Arrays.asList(
                "Register a listener that will get called for changes related to",
                "this class.  This listener will be called for any modifications",
                "of this class or any transitive children of this class.",
                "",
                "@param framework the framework instance ",
                "@param listener the listener functional object that should be",
                "called when changes occur related to the class.  This memory is",
                "owned by the caller and should be freed only after it has been",
                "unregistered."));
            out.println(aInIndent, "static void registerListener(");
            out.println(aInIndent + 1, "opflex::ofcore::OFFramework& framework,");
            out.println(aInIndent + 1, "opflex::modb::ObjectListener* listener)");
            out.println(aInIndent, "{");
            out.println(aInIndent + 1, "opflex::modb::mointernal");
            out.println(aInIndent + 2, "::MO::registerListener(framework, listener, CLASS_ID);");
            out.println(aInIndent, "}");
            out.println();
            out.printHeaderComment(aInIndent, Arrays.asList(
                "Unregister a listener from updates to this class.",
                "",
                "@param framework the framework instance ",
                "@param listener The listener to unregister."));
            out.println(aInIndent, "static void unregisterListener(");
            out.println(aInIndent + 1, "opflex::ofcore::OFFramework& framework,");
            out.println(aInIndent + 1, "opflex::modb::ObjectListener* listener)");
            out.println(aInIndent, "{");
            out.println(aInIndent + 1, "opflex::modb::mointernal");
            out.println(aInIndent + 2, "::MO::unregisterListener(framework, listener, CLASS_ID);");
            out.println(aInIndent, "}");
            out.println();
        }
    }

    private void genConstructor(int aInIdent, MClass aInClass)
    {
        String lclassName = getClassName(aInClass, false);
        String[] lComment = 
            {"Construct an instance of " + lclassName + ".",
             "This should not typically be called from user code."};
        out.printHeaderComment(aInIdent,lComment);

        if (aInClass.isConcrete())
        {
            out.println(aInIdent, aInClass.getLID().getName() + "(");
            out.println(aInIdent + 1, "opflex::ofcore::OFFramework& framework,");
            out.println(aInIdent + 1, "opflex::modb::class_id_t class_id,");
            out.println(aInIdent + 1, "const opflex::modb::URI& uri,");
            out.println(aInIdent + 1, "const std::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)");
            if (aInClass.getSuperclass() != null && aInClass.getSuperclass().isConcrete())
            {
                out.println(aInIdent + 1, ": " + Config.getProjName() + "::" +
                        getNamespace(aInClass.getSuperclass(), false) + "::" + aInClass.getSuperclass().getLID().getName() +
                        "(framework, class_id, uri, oi)  { }");
            }
            else
            {
                out.println(aInIdent + 1, ": MO(framework, class_id, uri, oi) { }");
            }

            out.println();
            out.printHeaderComment(aInIdent,lComment);
            out.println(aInIdent, aInClass.getLID().getName() + "(");
            out.println(aInIdent + 1, "opflex::ofcore::OFFramework& framework,");
            out.println(aInIdent + 1, "const opflex::modb::URI& uri,");
            out.println(aInIdent + 1, "const std::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)");
            if (aInClass.getSuperclass() != null && aInClass.getSuperclass().isConcrete())
            {
                out.println(aInIdent + 1, ": " + Config.getProjName() + "::" +
                    getNamespace(aInClass.getSuperclass(), false) + "::" + aInClass.getSuperclass().getLID().getName() +
                    "(framework, CLASS_ID, uri, oi) { }");
            }
            else
            {
                out.println(aInIdent + 1, ": MO(framework, CLASS_ID, uri, oi) { }");
            }
        }
        else
        {
            if (aInClass.hasSubclasses())
            {
                out.println(aInIdent, aInClass.getLID().getName() + "(");
                out.println(aInIdent + 1, "opflex::ofcore::OFFramework& framework,");
                out.println(aInIdent + 1, "opflex::modb::ClassId classId,");
                out.println(aInIdent + 1, "const opflex::modb::URI& uri,");
                out.println(aInIdent + 1, "const std::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)");
                if (aInClass.hasSuperclass())
                {
                    MClass lSuperclass = aInClass.getSuperclass();
                    out.println(aInIdent + 1, ": " + getClassName(lSuperclass,true) + "(framework, classId, uri, oi) {}");
                }
                else
                {
                    out.println(aInIdent + 1, ": MO(framework, classId, uri, oi) { }");
                }
            }
        }
    }
}
